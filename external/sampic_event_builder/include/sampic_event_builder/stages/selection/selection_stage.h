#pragma once

#include "sampic_event_builder/model/event.h"
#include "sampic_event_builder/pipeline/stage.h"

namespace sampic::event_builder {

enum class EventDecision { keep, reject_through_going };

class SelectionStage : public Stage {
public:
    virtual EventDecision select(const Event& event) const = 0;
};

} // namespace sampic::event_builder
