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
    const FrontendEventCollectorConfig& cfg,
    frontend::collector::FrontendDiagnostics& diagnostics)
    : FrontendCollectorMode(sampic_buffer, frontend_buffer, cfg, diagnostics),
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
    (void)sampic_buffer_.waitForNew(last_timestamp_, wait_timeout_);
    const auto t_wait_end = std::chrono::steady_clock::now();
    const auto wait_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_wait_end - t_wait_start);

    // ---------------------------------------------------------------------
    // Step 1: Retrieve new events
    // ---------------------------------------------------------------------
    auto new_events = sampic_buffer_.getSince(last_timestamp_);
    const auto now = std::chrono::steady_clock::now();
    std::chrono::microseconds group_build_us{0};

    if (!new_events.empty()) {
        if (spdlog::should_log(spdlog::level::trace)) {
            spdlog::trace("FrontendCollector: retrieved {} SampicEvents from buffer", new_events.size());
        }

        last_timestamp_ = new_events.back()->timestamp();

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

                // Check existing groups for a match
                for (auto& group : pending_groups_) {
                    if (group.hits.size() == 0)
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

                        group.last_activity = now;
                        placed = true;
                        break;
                    }
                }

                if (!placed) {
                    // Before creating new group, finalize old groups that are now too far away
                    // Any group whose hits are outside the time window from this new hit
                    // can never receive more hits, so finalize immediately
                    auto it = pending_groups_.begin();
                    while (it != pending_groups_.end()) {
                        if (it->hits.size() > 0) {
                            const double dt_from_new_hit =
                                std::abs(hit->FirstCellTimeStamp - it->hits.front()->FirstCellTimeStamp);

                            // If this group is beyond the time window, it's complete
                            if (dt_from_new_hit > time_window_ns_) {
                                ready_groups_.emplace_back(std::move(*it));
                                it = pending_groups_.erase(it);
                                continue;
                            } else {
                                // pending groups are time-ordered; newer groups will be closer in time
                                break;
                            }
                        }
                        ++it;
                    }

                    // Now create new group for this hit
                    PendingGroup g;
                    g.created = now;
                    g.last_activity = now;
                    g.parents.reserve(16);
                    g.hits.reserve(256);
                    g.parents.emplace_back(ev);
                    g.hits.emplace_back(hit);
                    pending_groups_.emplace_back(std::move(g));
                }
            }
        }
        const auto t_group_end = std::chrono::steady_clock::now();
        group_build_us =
            std::chrono::duration_cast<std::chrono::microseconds>(t_group_end - t_group_start);
    }

    // ---------------------------------------------------------------------
    // Step 3: Finalize groups that timed out (no activity for finalize_after_ms)
    // ---------------------------------------------------------------------
    const auto timeout_cutoff = now - finalize_after_;

    // Remove groups from front that have timed out
    while (!pending_groups_.empty() && pending_groups_.front().last_activity < timeout_cutoff) {
        ready_groups_.emplace_back(std::move(pending_groups_.front()));
        pending_groups_.pop_front();
    }

    // If no groups ready, return early (ready_groups_ already populated by immediate finalization above)
    if (ready_groups_.empty())
        return true;

    // ---------------------------------------------------------------------
    // Step 4: Emit finalized FrontendEvents
    // ---------------------------------------------------------------------
    auto groups_to_process = std::move(ready_groups_);
    ready_groups_.clear();

    emitted_events_.clear();
    emitted_events_.reserve(groups_to_process.size());
    const auto ready_group_count = groups_to_process.size();

    uint32_t total_hits = 0;
    const auto t_finalize_start = std::chrono::steady_clock::now();

    size_t produced_events = 0;
    size_t produced_hits = 0;

    for (auto& g : groups_to_process) {
        // Direct size check is faster than empty()
        if (g.hits.size() == 0)
            continue;
        total_hits += static_cast<uint32_t>(g.hits.size());
        produced_hits += g.hits.size();
        ++produced_events;

        if (cfg_.diagnostics.log_group_details && spdlog::should_log(spdlog::level::debug)) {
            spdlog::debug("Frontend grouping: hits={} parents={}",
                          g.hits.size(), g.parents.size());
        }

        parent_ptr_scratch_.clear();
        parent_ptr_scratch_.reserve(g.parents.size());
        for (const auto& parent_ref : g.parents) {
            parent_ptr_scratch_.push_back(parent_ref.get());
        }

        auto fev = std::make_shared<FrontendEvent>(g.created);

        // Zero-copy data bank (no temporary vector)
        auto data_bank =
            std::make_unique<FrontendEventBankData>(std::move(g.parents), g.hits);
        data_bank->setBankPrefix(mode_cfg_.data_bank_prefix);
        fev->addBank(std::move(data_bank));

        // Optional user-defined postprocessing
        fev->finalize();

        // Per-event timing bank
        auto event_timing_bank =
            std::make_unique<FrontendEventBankEventTiming>(g.created,
                                                           static_cast<uint32_t>(g.hits.size()),
                                                           parent_ptr_scratch_);
        event_timing_bank->setBankPrefix(mode_cfg_.event_timing_bank_prefix);
        fev->addBank(std::move(event_timing_bank));

        emitted_events_.emplace_back(std::move(fev));

        if (cfg_.diagnostics.log_hit_details && spdlog::should_log(spdlog::level::debug)) {
            size_t idx = 0;
            for (const HitStruct* hit : g.hits) {
                if (!hit)
                    continue;
                spdlog::debug("  hit[{}]: FEB={} sampic={} channel={} first_cell_ts(ns)={} amplitude={} TOT(ns)={}",
                              idx++, hit->FeBoardIndex, hit->SampicIndex,
                              hit->Channel, hit->FirstCellTimeStamp,
                              hit->Amplitude, hit->TOTValue);
            }
        }
    }
    groups_to_process.clear();

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

        auto collector_bank = std::make_unique<FrontendEventBankCollectorTiming>(rec);
        collector_bank->setBankPrefix(mode_cfg_.collector_timing_bank_prefix);
        emitted_events_.back()->addBank(std::move(collector_bank));
    }

    if (spdlog::should_log(spdlog::level::trace)) {
        spdlog::trace("FrontendCollector: pushing {} FrontendEvents to buffer (ready_groups.size={})",
                      emitted_events_.size(), ready_group_count);
    }

    for (const auto& fev : emitted_events_) {
        if (spdlog::should_log(spdlog::level::trace)) {
            spdlog::trace("FrontendCollector: pushing FrontendEvent with {} banks, {} hits",
                          fev->numBanks(), fev->totalDataSize());
        }
        frontend_buffer_.push(fev);
    }


    if (produced_events > 0) {
        diagnostics_.produced(produced_events, produced_hits, frontend_buffer_.size());
    }

    if (!new_events.empty()) {
        sampic_buffer_.pruneUpTo(last_timestamp_);
    }

    return true;
}
