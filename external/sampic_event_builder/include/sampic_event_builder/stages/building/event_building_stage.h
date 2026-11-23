#pragma once

#include "sampic_event_builder/model/event.h"
#include "sampic_event_builder/pipeline/stage.h"
#include <vector>

namespace sampic::event_builder {

class EventBuildingStage : public Stage {
public:
    /// Takes ownership so waveform buffers can move through event building.
    virtual std::vector<Event> build(std::vector<Hit> hits) = 0;
    /// Emit any complete or held-back events at an end-of-run boundary.
    virtual std::vector<Event> flush() { return {}; }
};

} // namespace sampic::event_builder
