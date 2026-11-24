#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

namespace sampic::event_builder {
namespace {

template<class StageType>
void require_stage(const std::unique_ptr<StageType>& stage, const char* name)
{
    if (!stage) {
        throw std::invalid_argument(std::string("missing pipeline stage: ") + name);
    }
}

} // namespace

EventBuilderPipeline::EventBuilderPipeline(
    std::unique_ptr<InputStage> input,
    std::unique_ptr<EventBuildingStage> builder,
    std::unique_ptr<SelectionStage> selection,
    std::unique_ptr<FinalizationStage> finalization)
    : input_(std::move(input)),
      builder_(std::move(builder)),
      selection_(std::move(selection)),
      finalization_(std::move(finalization))
{
    require_stage(input_, "input");
    require_stage(builder_, "builder");
    require_stage(selection_, "selection");
    require_stage(finalization_, "finalization");
}

std::vector<FinalizedEvent> EventBuilderPipeline::run_once()
{
    auto hits = input_->read_available();
    return process_owned_hits(std::move(hits));
}

std::vector<FinalizedEvent> EventBuilderPipeline::process_hits(std::span<const Hit> input)
{
    return process_owned_hits(std::vector<Hit>(input.begin(), input.end()));
}

std::vector<FinalizedEvent> EventBuilderPipeline::process_owned_hits(
    std::vector<Hit> hits, bool final)
{

    std::uint64_t bytes = 0;
    for (const auto& hit : hits) bytes += hit.sample_count() * sizeof(std::int16_t);
    const auto hit_count = hits.size();
    const auto start = std::chrono::steady_clock::now();
    auto events = final ? builder_->finish(std::move(hits)) : builder_->build(std::move(hits));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    metrics_.event_building.hits += hit_count;
    metrics_.event_building.input_bytes += bytes;
    metrics_.event_building.output_bytes += bytes;
    metrics_.event_building.processing_nanoseconds +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    return process_events(std::move(events));
}

std::vector<FinalizedEvent> EventBuilderPipeline::drain_and_flush() {
    return process_owned_hits(input_->read_available(),true);
}

std::vector<FinalizedEvent> EventBuilderPipeline::flush()
{
    return process_events(builder_->flush());
}

std::vector<FinalizedEvent> EventBuilderPipeline::process_events(
    std::vector<Event> events)
{
    std::vector<FinalizedEvent> output;
    for (auto& source : events) {
        std::uint64_t raw_bytes = 0;
        for (const auto& hit : source.hits)
            raw_bytes += hit.sample_count() * sizeof(std::int16_t);
        auto start = std::chrono::steady_clock::now();
        const auto initial_hits = source.hits.size();
        selection_->apply(source);
        auto elapsed = std::chrono::steady_clock::now() - start;
        metrics_.selection.hits += source.hits.size();
        metrics_.selection.input_bytes += raw_bytes;
        for (const auto& hit : source.hits)
            metrics_.selection.output_bytes += hit.sample_count() * sizeof(std::int16_t);
        metrics_.selection.processing_nanoseconds +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (source.hits.size() < initial_hits) ++metrics_.rejected_events;
        else ++metrics_.selected_events;

        start = std::chrono::steady_clock::now();
        auto finalized = finalization_->finalize(source);
        elapsed = std::chrono::steady_clock::now() - start;
        metrics_.finalization.hits += source.hits.size();
        for (const auto& hit : source.hits)
            metrics_.finalization.input_bytes += hit.sample_count() * sizeof(std::int16_t);
        metrics_.finalization.output_bytes += finalized.product_bank.size();
        metrics_.finalization.processing_nanoseconds +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        output.push_back(std::move(finalized));
    }
    return output;
}

void EventBuilderPipeline::replace_input(std::unique_ptr<InputStage> stage)
{
    require_stage(stage, "input");
    input_ = std::move(stage);
}

void EventBuilderPipeline::replace_builder(std::unique_ptr<EventBuildingStage> stage)
{
    require_stage(stage, "builder");
    builder_ = std::move(stage);
}

void EventBuilderPipeline::replace_selection(std::unique_ptr<SelectionStage> stage)
{
    require_stage(stage, "selection");
    selection_ = std::move(stage);
}

void EventBuilderPipeline::replace_finalization(std::unique_ptr<FinalizationStage> stage)
{
    require_stage(stage, "finalization");
    finalization_ = std::move(stage);
}

} // namespace sampic::event_builder
