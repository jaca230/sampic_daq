#pragma once

#include "sampic_event_builder/model/event.h"
#include "sampic_event_builder/pipeline/stage.h"
#include <vector>
#include <iterator>
#include <chrono>
#include <optional>

namespace sampic::event_builder {

class EventBuildingStage : public Stage {
public:
    /// Takes ownership so waveform buffers can move through event building.
    virtual std::vector<Event> build(std::vector<Hit> hits) = 0;
    /// Emit any complete or held-back events at an end-of-run boundary.
    virtual std::vector<Event> flush() { return {}; }
    /// Consume final input and flush. Periodic modes coalesce it into one final window.
    virtual std::vector<Event> finish(std::vector<Hit> hits) {
        auto events=build(std::move(hits));
        auto tail=flush();
        events.insert(events.end(),std::make_move_iterator(tail.begin()),std::make_move_iterator(tail.end()));
        return events;
    }
    virtual std::optional<std::chrono::steady_clock::time_point> readout_deadline() const { return {}; }
};

} // namespace sampic::event_builder
