#pragma once
#include "sampic_event_builder/stages/fitting/fitting_stage.h"

namespace sampic::event_builder {
class PassthroughFittingStage final : public FittingStage {
public:
    void fit(PipelineEvent& event) const override;
    std::string_view type() const override { return "none"; }
};
} // namespace sampic::event_builder
