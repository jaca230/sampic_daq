#include "processing/sampic_processing/collector/modes/periodic_readout/periodic_readout_mode.h"
#include "processing/sampic_processing/collector/banks/frontend_event_hit_banks.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_readout_tick.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "core/registry/mode/mode_auto_registration.h"
#include <limits>
#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(FrontendCollectorModeRegistry, FrontendCollectorModePeriodicReadout,
    FrontendCollectorModePeriodicReadoutConfig, "periodic_readout",
    "Software TTC: periodically drain arrived hits, including empty readouts",
    [](const FrontendCollectorModePeriodicReadoutConfig& config) {
        FrontendCollectorModePeriodicReadout::validate(config);
    });
void FrontendCollectorModePeriodicReadout::validate(const FrontendCollectorModePeriodicReadoutConfig& config) {
    if (config.readout_period_ns == 0 || config.readout_period_ns > std::numeric_limits<std::int64_t>::max() ||
        config.max_pending_readouts == 0) throw std::invalid_argument("invalid readout period or pending limit");
    validateFrontendHitBanks(config, {{true, config.readout_bank_prefix},
        {config.event_timing_bank_enabled, config.event_timing_bank_prefix}});
}
FrontendCollectorModePeriodicReadout::FrontendCollectorModePeriodicReadout(
    FrontendCollectorModeContext& context, FrontendCollectorModePeriodicReadoutConfig config, Now now)
    : FrontendCollectorMode(context), config_(std::move(config)), now_(std::move(now)) {
    validate(config_);
}
void FrontendCollectorModePeriodicReadout::onStart() {
    if (!zero_) {
        zero_ = now_();
        spdlog::info("Periodic software readout started: period={} ns, host steady-clock origin, empty_readouts={}",
            config_.readout_period_ns, config_.emit_empty_readouts);
    }
}
void FrontendCollectorModePeriodicReadout::receive() {
    auto events = sampic_buffer_.getSince(last_input_);
    for (const auto& parent : events) {
        if (!parent || !parent->data()) continue;
        const auto& event = *parent->data();
        if (event.NbOfHitsInEvent < 0 || event.NbOfHitsInEvent > MAX_EXPECTED_FRAMES)
            throw std::runtime_error("invalid vendor hit count in periodic_readout");
        if (event.NbOfHitsInEvent) {
            // Host time selects the readout. Preserve vendor timestamps exactly;
            // hardware-counter rollover reconstruction belongs to offline analysis.
            parents_.push_back(parent);
            for (int i = 0; i < event.NbOfHitsInEvent; ++i) hits_.push_back(&event.Hit[i]);
        }
    }
    if (!events.empty()) {
        last_input_ = events.back()->timestamp();
        sampic_buffer_.pruneUpTo(last_input_);
    }
}
void FrontendCollectorModePeriodicReadout::readout(bool final) {
    const auto now = now_();
    if (now < *zero_) throw std::runtime_error("software readout steady clock moved backwards");
    const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - *zero_).count());
    const auto completed = elapsed / config_.readout_period_ns;
    if (completed < next_tick_) throw std::runtime_error("software readout clock moved backwards");
    if (!final && completed <= next_tick_) return;
    const auto start = next_tick_ * config_.readout_period_ns;
    if (final && elapsed == start && hits_.empty()) return;
    if (!config_.emit_empty_readouts && hits_.empty()) { next_tick_ = completed; return; }
    if (ready_.size() >= config_.max_pending_readouts)
        throw std::runtime_error("periodic readout output backlog limit exceeded");
    if (hits_.size() > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("too many hits in a software readout");
    FrontendEventBankReadoutTick::Record record;
    record.first_tick = next_tick_;
    record.ticks_elapsed = completed - next_tick_;
    record.start_ns = start;
    record.end_ns = final ? elapsed : completed * config_.readout_period_ns;
    record.actual_readout_ns = elapsed;
    record.hit_count = static_cast<std::uint32_t>(hits_.size());
    record.parent_count = static_cast<std::uint32_t>(parents_.size());
    if (!final && (elapsed > completed * config_.readout_period_ns || record.ticks_elapsed > 1))
        record.flags |= FrontendEventBankReadoutTick::LATE;
    if (record.ticks_elapsed > 1) record.flags |= FrontendEventBankReadoutTick::COALESCED;
    if (final) record.flags |= FrontendEventBankReadoutTick::FINAL_READOUT;
    auto event = std::make_shared<FrontendEvent>(now);
    addFrontendHitBanks(*event, parents_, hits_, config_);
    auto bank = std::make_unique<FrontendEventBankReadoutTick>(record);
    bank->setBankPrefix(config_.readout_bank_prefix);event->addBank(std::move(bank));
    if (config_.event_timing_bank_enabled) {
        std::vector<SampicEvent*> parents;
        for (const auto& parent : parents_) parents.push_back(parent.get());
        event->addBank(std::make_unique<FrontendEventBankEventTiming>(now, record.hit_count, parents, config_.event_timing_bank_prefix));
    }
    event->finalize();ready_.push_back(std::move(event));
    diagnostics_.produced(1, hits_.size(), frontend_buffer_.size());
    parents_.clear();hits_.clear();next_tick_ = completed;
}
void FrontendCollectorModePeriodicReadout::send() {
    while (!ready_.empty() && frontend_buffer_.tryPush(ready_.front())) ready_.pop_front();
}
bool FrontendCollectorModePeriodicReadout::collect() {
    if (flushed_) { send();return true; }
    onStart();send();receive();readout(false);send();return true;
}
bool FrontendCollectorModePeriodicReadout::flush() {
    onStart();send();
    if (!flushed_) { receive();readout(true);flushed_ = true; }
    send();return true;
}
