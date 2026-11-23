#pragma once
#include "sampic_event_builder/stages/selection/selection_stage.h"

namespace sampic::event_builder {
class PassthroughSelectionStage final : public SelectionStage {
public:
    EventDecision select(const Event&) const override { return EventDecision::keep; }
    std::string_view type() const override { return "passthrough"; }
};
} // namespace sampic::event_builder
