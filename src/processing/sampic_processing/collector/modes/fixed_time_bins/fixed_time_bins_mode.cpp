#include "integration/sampic/collector/extended_timestamp_parent.h"
#include "processing/sampic_processing/collector/modes/fixed_time_bins/fixed_time_bins_mode.h"
#include "processing/sampic_processing/collector/banks/frontend_event_hit_banks.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_time_bin.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "core/registry/mode/mode_auto_registration.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(FrontendCollectorModeRegistry, FrontendCollectorModeFixedTimeBins,
    FrontendCollectorModeFixedTimeBinsConfig, "fixed_time_bins",
    "Fixed hardware-time bins, including empty bins",
    [](const FrontendCollectorModeFixedTimeBinsConfig& config) {
        FrontendCollectorModeFixedTimeBins::validate(config);
    });

void FrontendCollectorModeFixedTimeBins::validate(const FrontendCollectorModeFixedTimeBinsConfig& config) {
    if (!std::isfinite(config.bin_width_ns) || config.bin_width_ns <= 0 ||
        !std::isfinite(config.bin_origin_ns) || !std::isfinite(config.allowed_lateness_ns) ||
        config.allowed_lateness_ns < 0 || config.idle_flush_ms == 0 ||
        config.wait_timeout_ms == 0 || config.max_pending_bins == 0 || config.max_bins_per_collect == 0)
        throw std::invalid_argument("invalid fixed_time_bins width, lateness, timeout or capacity");
    validateFrontendHitBanks(config, {{true, config.bin_bank_prefix},
        {config.event_timing_bank_enabled, config.event_timing_bank_prefix}});
}

FrontendCollectorModeFixedTimeBins::FrontendCollectorModeFixedTimeBins(
    FrontendCollectorModeContext& context, FrontendCollectorModeFixedTimeBinsConfig config)
    : FrontendCollectorMode(context), config_(std::move(config)) {
    validate(config_);
    spdlog::info("Fixed-time bins initialized: width={} ns, origin={} ns, lateness={} ns, empty_bins={}, verbose_banks={}",
        config_.bin_width_ns, config_.bin_origin_ns, config_.allowed_lateness_ns,
        config_.emit_empty_bins, config_.verbose_banks);
}

std::int64_t FrontendCollectorModeFixedTimeBins::indexFor(double timestamp_ns) const {
    if (!std::isfinite(timestamp_ns)) throw std::runtime_error("nonfinite hit timestamp in fixed_time_bins");
    const long double index = std::floor((static_cast<long double>(timestamp_ns) - config_.bin_origin_ns) / config_.bin_width_ns);
    // Leave room for an exclusive endpoint, increment, and watermark subtraction.
    if (index <= static_cast<long double>(std::numeric_limits<std::int64_t>::min()) ||
        index >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()) - 1)
        throw std::runtime_error("fixed_time_bins timestamp/index range exceeded");
    return static_cast<std::int64_t>(index);
}

std::pair<double, double> FrontendCollectorModeFixedTimeBins::bounds(std::int64_t index) const {
    const long double start = static_cast<long double>(config_.bin_origin_ns) +
                              static_cast<long double>(index) * config_.bin_width_ns;
    const double a = static_cast<double>(start), b = static_cast<double>(start + config_.bin_width_ns);
    if (!std::isfinite(a) || !std::isfinite(b) || !(b > a))
        throw std::runtime_error("fixed_time_bins boundaries exceed timestamp precision/range");
    return {a, b};
}

void FrontendCollectorModeFixedTimeBins::advanceReady(std::int64_t index) {
    if (!ready_through_ || index > *ready_through_) ready_through_ = index;
}

bool FrontendCollectorModeFixedTimeBins::collect() {
    const bool backlog = next_bin_ && ready_through_ && *next_bin_ <= *ready_through_;
    if (!backlog) sampic_buffer_.waitForNew(last_timestamp_, std::chrono::milliseconds(config_.wait_timeout_ms));
    auto events = sampic_buffer_.getSince(last_timestamp_);
    const auto now = Clock::now();
    for (const auto& parent : events) {
        if (!parent || !parent->data()) continue;
        const auto extended = extendParentTimestamps(parent, timestamp_unwrapper_);
        const auto& hit_parent = extended.owner;
        const auto* hit_data = extended.hits;
        for (int i = 0; i < extended.hit_count; ++i) {
            const auto* hit = &hit_data[i];
            const auto index = indexFor(hit->FirstCellTimeStamp);
            if (emitted_ && next_bin_ && index < *next_bin_)
                throw std::runtime_error("late hit targets an already emitted time bin; increase allowed_lateness_ns/idle_flush_ms");
            if (!next_bin_ || (!emitted_ && index < *next_bin_)) next_bin_ = index;
            if (!last_bin_ || index > *last_bin_) last_bin_ = index;
            max_timestamp_ns_ = std::max(max_timestamp_ns_, hit->FirstCellTimeStamp);
            auto [it, inserted] = bins_.try_emplace(index);
            if (inserted && bins_.size() > config_.max_pending_bins)
                throw std::runtime_error("fixed_time_bins pending bin limit exceeded");
            auto& bin = it->second;
            bin.hits.push_back(hit);
            // Each vendor event contributes at most once to a bin's parent list.
            if (bin.parents.empty() || bin.parents.back().get() != parent.get()) bin.parents.push_back(hit_parent);
            last_input_ = now;
        }
    }
    if (!events.empty()) {
        last_timestamp_ = events.back()->timestamp();
        sampic_buffer_.pruneUpTo(last_timestamp_); // Pending bins retain parent ownership.
    }
    if (last_bin_) {
        const double watermark = max_timestamp_ns_ - config_.allowed_lateness_ns;
        advanceReady(indexFor(watermark) - 1); // Close only bins whose exclusive end <= watermark.
        if (now - last_input_ >= std::chrono::milliseconds(config_.idle_flush_ms)) advanceReady(*last_bin_);
    }
    return emitReady(config_.max_bins_per_collect);
}

bool FrontendCollectorModeFixedTimeBins::emitReady(std::size_t limit) {
    std::size_t produced_events = 0, produced_hits = 0;
    while (next_bin_ && ready_through_ && *next_bin_ <= *ready_through_ && produced_events < limit) {
        if (!config_.emit_empty_bins) {
            auto next = bins_.lower_bound(*next_bin_);
            if (next == bins_.end() || next->first > *ready_through_) {
                *next_bin_ = *ready_through_ + 1;
                break;
            }
            *next_bin_ = next->first;
        }
        const auto index = *next_bin_;
        const auto [start, end] = bounds(index);
        auto found = bins_.find(index);
        const Bin empty;
        const auto& bin = found == bins_.end() ? empty : found->second;
        if (bin.hits.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("too many hits in a fixed time bin");
        const auto host_time = Clock::now();
        auto event = std::make_shared<FrontendEvent>(host_time);
        addFrontendHitBanks(*event, bin.parents, bin.hits, config_);
        FrontendEventBankTimeBin::Record metadata;
        metadata.hit_count = static_cast<std::uint32_t>(bin.hits.size());
        metadata.bin_index = index; metadata.start_ns = start; metadata.end_ns = end;
        auto bank = std::make_unique<FrontendEventBankTimeBin>(metadata);
        bank->setBankPrefix(config_.bin_bank_prefix);
        event->addBank(std::move(bank));
        if (config_.event_timing_bank_enabled) {
            std::vector<SampicEvent*> parents;
            for (const auto& parent : bin.parents) parents.push_back(parent.get());
            event->addBank(std::make_unique<FrontendEventBankEventTiming>(host_time,
                metadata.hit_count, parents, config_.event_timing_bank_prefix));
        }
        event->finalize();
        if (!frontend_buffer_.tryPush(event)) break; // Keep this bin pending; never discard earlier events.
        produced_hits += bin.hits.size();
        ++produced_events;
        if (found != bins_.end()) bins_.erase(found);
        *next_bin_ = index + 1;
        emitted_ = true;
    }
    if (produced_events) diagnostics_.produced(produced_events, produced_hits, frontend_buffer_.size());
    return true;
}

bool FrontendCollectorModeFixedTimeBins::flush() {
    if (last_bin_) advanceReady(*last_bin_);
    return emitReady(config_.max_bins_per_collect);
}
