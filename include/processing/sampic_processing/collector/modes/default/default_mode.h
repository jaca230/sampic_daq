#ifndef FRONTEND_COLLECTOR_MODE_DEFAULT_H
#define FRONTEND_COLLECTOR_MODE_DEFAULT_H

#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/default/default_config.h"
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"

#include <deque>
#include <vector>
#include <memory>
#include <chrono>

/**
 * @class FrontendCollectorModeDefault
 * @brief Groups temporally close hits from SAMPIC events into FrontendEvents.
 *
 * Performs low-overhead grouping and packaging of events with minimal
 * allocations, intended for high-rate operation.
 */
class FrontendCollectorModeDefault : public FrontendCollectorMode {
public:
    FrontendCollectorModeDefault(
        FrontendCollectorModeContext& context,
        FrontendCollectorModeDefaultConfig config);

    bool collect() override;
    bool flush() override;

private:
    struct PendingGroup {
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point last_activity;  ///< Last time a hit was added
        std::vector<std::shared_ptr<SampicEvent>> parents;  ///< Shared ownership of SampicEvents
        std::vector<const HitStruct*> hits;                 ///< Direct pointers to hits (zero-copy)
    };

    // Persistent working sets to avoid per-iteration allocations
    std::deque<PendingGroup> pending_groups_;
    std::vector<PendingGroup> ready_groups_;
    std::vector<std::shared_ptr<FrontendEvent>> emitted_events_;
    mutable std::vector<SampicEvent*> parent_ptr_scratch_;

    std::chrono::steady_clock::time_point last_timestamp_{std::chrono::steady_clock::time_point::min()};

    FrontendCollectorModeDefaultConfig mode_cfg_;
    std::chrono::milliseconds finalize_after_;
    std::chrono::milliseconds wait_timeout_;
    double time_window_ns_;

    bool emitReadyGroups(
        std::chrono::steady_clock::time_point started,
        std::chrono::microseconds wait_time,
        std::chrono::microseconds group_build_time);
};

#endif // FRONTEND_COLLECTOR_MODE_DEFAULT_H
