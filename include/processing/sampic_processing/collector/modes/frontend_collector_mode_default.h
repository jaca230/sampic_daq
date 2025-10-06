#ifndef FRONTEND_COLLECTOR_MODE_DEFAULT_H
#define FRONTEND_COLLECTOR_MODE_DEFAULT_H

#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"

#include <deque>
#include <vector>
#include <memory>
#include <chrono>

/// Default frontend collector mode:
/// Groups hits from SAMPIC events based on time proximity.
class FrontendCollectorModeDefault : public FrontendCollectorMode {
public:
    FrontendCollectorModeDefault(FrontendEventBuffer& buffer,
                                 const FrontendEventCollectorConfig& cfg);

    /// Transform new SAMPIC events into FrontendEvents.
    bool collect(const std::vector<TimestampedSampicEvent>& new_events) override;

private:
    struct PendingGroup {
        std::chrono::steady_clock::time_point created;
        std::vector<std::shared_ptr<const EventStruct>> parents;
        std::vector<const HitStruct*> hits;
    };

    std::deque<PendingGroup> pending_groups_;
    std::chrono::milliseconds finalize_after_;
    double time_window_ns_;
};

#endif // FRONTEND_COLLECTOR_MODE_DEFAULT_H
