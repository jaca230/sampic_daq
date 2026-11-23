#pragma once

#include "sampic_event_builder/pipeline/pipeline_event.h"
#include "sampic_event_builder/stages/building/event_building_stage.h"
#include "sampic_event_builder/stages/finalization/finalization_stage.h"
#include "sampic_event_builder/stages/input/input_stage.h"
#include "sampic_event_builder/stages/selection/selection_stage.h"

#include <memory>
#include <chrono>
#include <span>
#include <vector>

namespace sampic::event_builder {

struct PipelineStageTotals {
    std::uint64_t hits = 0;
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    std::uint64_t processing_nanoseconds = 0;
};

struct PipelineMetrics {
    PipelineStageTotals event_building;
    PipelineStageTotals selection;
    PipelineStageTotals finalization;
    std::uint64_t selected_events = 0;
    std::uint64_t rejected_events = 0;
};

class EventBuilderPipeline {
public:
    EventBuilderPipeline(std::unique_ptr<InputStage> input,
                         std::unique_ptr<EventBuildingStage> builder,
                         std::unique_ptr<SelectionStage> selection,
                         std::unique_ptr<FinalizationStage> finalization);
    std::vector<FinalizedEvent> run_once();
    std::vector<FinalizedEvent> process_hits(std::span<const Hit> hits);
    std::vector<FinalizedEvent> flush();
    [[nodiscard]] const PipelineMetrics& metrics() const noexcept { return metrics_; }
    void reset_metrics() noexcept { metrics_ = {}; }

    void replace_input(std::unique_ptr<InputStage> stage);
    void replace_builder(std::unique_ptr<EventBuildingStage> stage);
    void replace_selection(std::unique_ptr<SelectionStage> stage);
    void replace_finalization(std::unique_ptr<FinalizationStage> stage);

private:
    std::unique_ptr<InputStage> input_;
    std::unique_ptr<EventBuildingStage> builder_;
    std::unique_ptr<SelectionStage> selection_;
    std::unique_ptr<FinalizationStage> finalization_;
    PipelineMetrics metrics_;

    std::vector<FinalizedEvent> process_events(std::vector<Event> events);
    std::vector<FinalizedEvent> process_owned_hits(std::vector<Hit> hits);
};

} // namespace sampic::event_builder
