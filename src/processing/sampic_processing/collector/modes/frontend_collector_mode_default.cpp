#include "processing/sampic_processing/collector/modes/frontend_collector_mode_default.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

FrontendCollectorModeDefault::FrontendCollectorModeDefault(
    FrontendEventBuffer& buffer,
    const FrontendEventCollectorConfig& cfg)
    : FrontendCollectorMode(buffer, cfg)
{
    const auto& mode_cfg = cfg.default_mode.at("default_mode");

    time_window_ns_ = mode_cfg.time_window_ns;
    finalize_after_ = std::chrono::milliseconds(
        static_cast<int>(mode_cfg.finalize_after_ms));

    spdlog::info("FrontendCollectorModeDefault initialized: "
                 "time_window_ns = {}, finalize_after_ms = {}",
                 time_window_ns_, mode_cfg.finalize_after_ms);
}

// ---------------------------------------------------------------------------
// collect()
// ---------------------------------------------------------------------------
// Called periodically by FrontendEventCollector. It receives a vector of new
// SAMPIC events, groups hits that are temporally close, and ships complete
// groups as FrontendEvents to the buffer.
// ---------------------------------------------------------------------------
bool FrontendCollectorModeDefault::collect(
    const std::vector<TimestampedSampicEvent>& new_events)
{
    if (new_events.empty())
        return true;

    const auto now = std::chrono::steady_clock::now();
    bool emitted_any = false;

    // -----------------------------------------------------------------------
    // Step 1: Process all new events and group hits by temporal proximity
    // -----------------------------------------------------------------------
    for (const auto& evt : new_events)
    {
        if (!evt.event) continue;
        auto parent = evt.event; // shared_ptr<EventStruct>

        // Loop over all hits in this EventStruct
        for (int i = 0; i < parent->NbOfHitsInEvent; ++i)
        {
            const HitStruct* hit = &parent->Hit[i];
            bool placed = false;

            // Try to fit this hit into an existing pending group
            for (auto& group : pending_groups_)
            {
                if (group.hits.empty()) continue;

                const HitStruct* ref = group.hits.front();
                double dt_ns = std::abs(hit->FirstCellTimeStamp - ref->FirstCellTimeStamp);

                if (dt_ns <= time_window_ns_)
                {
                    // Same temporal cluster → add to this group
                    group.hits.push_back(hit);

                    // Only store this parent once per group
                    bool has_parent = std::any_of(
                        group.parents.begin(), group.parents.end(),
                        [&](const std::shared_ptr<const EventStruct>& p) {
                            return p.get() == parent.get();
                        });

                    if (!has_parent)
                        group.parents.push_back(parent);

                    placed = true;
                    break;
                }
            }

            // No matching group found → create a new one
            if (!placed)
            {
                PendingGroup g;
                g.created = now;
                g.parents.push_back(parent);
                g.hits.push_back(hit);
                pending_groups_.push_back(std::move(g));
            }
        }
    }

    // -----------------------------------------------------------------------
    // Step 2: Check for groups that have aged beyond finalize_after_
    // -----------------------------------------------------------------------
    std::vector<PendingGroup> ready_groups;
    const auto cutoff = now - finalize_after_;

    while (!pending_groups_.empty() &&
           pending_groups_.front().created < cutoff)
    {
        ready_groups.push_back(std::move(pending_groups_.front()));
        pending_groups_.pop_front();
    }

    // -----------------------------------------------------------------------
    // Step 3: Convert each ready group into a FrontendEvent
    // -----------------------------------------------------------------------
    for (auto& g : ready_groups)
    {
        if (g.hits.empty()) continue;

        auto fev = std::make_shared<FrontendEvent>();
        fev->setTimestamp(g.created);

        // Bank takes ownership of parent shared_ptrs to maintain hit lifetimes
        auto bank = std::make_shared<FrontendEventBankData>(
            g.parents, g.hits);

        fev->addBank(bank);
        fev->finalize();

        buffer_.push(fev);
        emitted_any = true;

        spdlog::debug("FrontendCollectorModeDefault: emitted event with {} hits "
                      "from {} parent EventStruct(s)",
                      g.hits.size(), g.parents.size());
    }

    // -----------------------------------------------------------------------
    // Step 4: Return whether anything was produced
    // -----------------------------------------------------------------------
    return true; //always return true for now
}
