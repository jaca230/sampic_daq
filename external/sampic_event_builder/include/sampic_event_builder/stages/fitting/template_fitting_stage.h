#pragma once
#include "sampic_event_builder/stages/fitting/fitting_stage.h"
#include "template_fitter/template_fitter.h"

namespace sampic::event_builder {
struct TemplateFittingStageConfig {
    std::uint64_t template_id = 0;
    double maximum_reduced_chi_squared = 5.0;
};
class TemplateFittingStage final : public FittingStage {
public:
    TemplateFittingStage(TemplateFittingStageConfig config,
                         const waveform::fitting::TemplateFitter& fitter)
        : config_(config), fitter_(fitter) {}
    void fit(PipelineEvent& event) const override;
    std::string_view type() const override { return "template"; }
private:
    TemplateFittingStageConfig config_;
    const waveform::fitting::TemplateFitter& fitter_;
};
} // namespace sampic::event_builder
