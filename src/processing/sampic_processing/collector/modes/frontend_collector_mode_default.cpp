#include "processing/sampic_processing/collector/modes/frontend_collector_mode_default.h"
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_collector_timing.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

FrontendCollectorModeDefault::FrontendCollectorModeDefault(
    SampicEventBuffer& sampic_buffer,
    FrontendEventBuffer& frontend_buffer,
    const FrontendEventCollectorConfig& cfg)
    : FrontendCollectorMode(sampic_buffer, frontend_buffer, cfg),
      mode_cfg_(cfg.default_mode)
{
    time_window_ns_ = mode_cfg_.time_window_ns;
    finalize_after_ = std::chrono::milliseconds(static_cast<int>(mode_cfg_.finalize_after_ms));
    wait_timeout_   = std::chrono::milliseconds(mode_cfg_.wait_timeout_ms);

    ready_groups_.reserve(32);
    emitted_events_.reserve(32);

    spdlog::info("FrontendCollectorModeDefault initialized "
                 "(time_window_ns={}, finalize_after_ms={}, wait_timeout_ms={})",
                 time_window_ns_,
                 mode_cfg_.finalize_after_ms,
                 mode_cfg_.wait_timeout_ms);
}

/**
 * @brief Perform one collector iteration. Zero-copy and allocation-minimized.
 */
bool FrontendCollectorModeDefault::collect()
{
    const auto t_start = std::chrono::steady_clock::now();

    // ---------------------------------------------------------------------
    // Step 0: Wait for new SampicEvents
    // ---------------------------------------------------------------------
    const auto t_wait_start = std::chrono::steady_clock::now();
    if (!sampic_buffer_.waitForNew(last_timestamp_, wait_timeout_))
        return true; // timeout is fine
    const auto t_wait_end = std::chrono::steady_clock::now();
    const auto wait_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_wait_end - t_wait_start);

    // ---------------------------------------------------------------------
    // Step 1: Retrieve new events
    // ---------------------------------------------------------------------
    auto new_events = sampic_buffer_.getSince(last_timestamp_);
    if (new_events.empty())
        return true;

    last_timestamp_ = new_events.back()->timestamp();
    const auto now = std::chrono::steady_clock::now();

    // ---------------------------------------------------------------------
    // Step 2: Group hits by temporal proximity
    // ---------------------------------------------------------------------
    const auto t_group_start = std::chrono::steady_clock::now();

    for (const auto& ev : new_events) {
        if (!ev || !ev->data())
            continue;
        const auto parent = ev->data();

        for (int i = 0; i < parent->NbOfHitsInEvent; ++i) {
            const HitStruct* hit = &parent->Hit[i];
            bool placed = false;

            for (auto& group : pending_groups_) {
                if (group.hits.empty())
                    continue;

                const double dt_ns =
                    std::abs(hit->FirstCellTimeStamp - group.hits.front()->FirstCellTimeStamp);
                if (dt_ns <= time_window_ns_) {
                    group.hits.emplace_back(hit);

                    if (std::none_of(group.parents.begin(), group.parents.end(),
                                     [&](const std::shared_ptr<SampicEvent>& p) {
                                         return p.get() == ev.get();
                                     })) {
                        group.parents.emplace_back(ev);
                    }

                    placed = true;
                    break;
                }
            }

            if (!placed) {
                PendingGroup g;
                g.created = now;
                g.parents.reserve(16);
                g.hits.reserve(256);
                g.parents.emplace_back(ev);
                g.hits.emplace_back(hit);
                pending_groups_.emplace_back(std::move(g));
            }
        }
    }

    const auto t_group_end = std::chrono::steady_clock::now();
    const auto group_build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_group_end - t_group_start);

    // ---------------------------------------------------------------------
    // Step 3: Finalize aged groups
    // ---------------------------------------------------------------------
    const auto cutoff = now - finalize_after_;
    ready_groups_.clear();

    while (!pending_groups_.empty() && pending_groups_.front().created < cutoff) {
        ready_groups_.emplace_back(std::move(pending_groups_.front()));
        pending_groups_.pop_front();
    }

    if (ready_groups_.empty())
        return true;

    // ---------------------------------------------------------------------
    // Step 4: Emit finalized FrontendEvents
    // ---------------------------------------------------------------------
    emitted_events_.clear();
    emitted_events_.reserve(ready_groups_.size());

    uint32_t total_hits = 0;
    const auto t_finalize_start = std::chrono::steady_clock::now();

    for (auto& g : ready_groups_) {
        if (g.hits.empty())
            continue;
        total_hits += static_cast<uint32_t>(g.hits.size());

        auto fev = std::make_shared<FrontendEvent>(g.created);

        // Zero-copy data bank (no temporary vector)
        auto data_bank = std::make_shared<FrontendEventBankData>(g.parents, g.hits);
        data_bank->setBankPrefix(mode_cfg_.data_bank_prefix);
        fev->addBank(data_bank);

        // Optional user-defined postprocessing
        fev->finalize();

        // Per-event timing bank
        auto event_timing_bank =
            std::make_shared<FrontendEventBankEventTiming>(g.created,
                                                           static_cast<uint32_t>(g.hits.size()),
                                                           g.parents);
        event_timing_bank->setBankPrefix(mode_cfg_.event_timing_bank_prefix);
        fev->addBank(event_timing_bank);

        frontend_buffer_.push(fev);
        emitted_events_.emplace_back(std::move(fev));
    }

    const auto t_finalize_end = std::chrono::steady_clock::now();
    const auto finalize_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_finalize_end - t_finalize_start);
    const auto total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_finalize_end - t_start);

    // ---------------------------------------------------------------------
    // Step 5: Collector timing bank (last event only)
    // ---------------------------------------------------------------------
    if (!emitted_events_.empty()) {
        FrontendEventBankCollectorTiming::Record rec{};
        rec.collector_timestamp_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                t_start.time_since_epoch()).count());
        rec.n_events       = static_cast<uint32_t>(emitted_events_.size());
        rec.total_hits     = total_hits;
        rec.wait_us        = static_cast<uint32_t>(wait_us.count());
        rec.group_build_us = static_cast<uint32_t>(group_build_us.count());
        rec.finalize_us    = static_cast<uint32_t>(finalize_us.count());
        rec.total_us       = static_cast<uint32_t>(total_us.count());

        auto collector_bank = std::make_shared<FrontendEventBankCollectorTiming>(rec);
        collector_bank->setBankPrefix(mode_cfg_.collector_timing_bank_prefix);
        emitted_events_.back()->addBank(collector_bank);
    }

    spdlog::debug("FrontendCollectorModeDefault: emitted {} FrontendEvents ({} total hits, {} µs total)",
                  emitted_events_.size(), total_hits, total_us.count());
    spdlog::trace("FrontendCollectorModeDefault timing: wait={}us, group={}us, finalize={}us, total={}us",
                  wait_us.count(), group_build_us.count(), finalize_us.count(), total_us.count());

    return true;
}
