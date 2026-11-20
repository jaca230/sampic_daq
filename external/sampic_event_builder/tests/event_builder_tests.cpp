#include "sampic_event_builder/stages/building/fixed_time_bin_event_builder.h"
#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"
#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/serialization/product_bank_decoder.h"
#include "sampic_event_builder/stages/compression/residual_compression_stage.h"
#include "sampic_event_builder/stages/compression/uncompressed_stage.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/stages/fitting/passthrough_fitting_stage.h"
#include "sampic_event_builder/stages/fitting/template_fitting_stage.h"
#include "sampic_event_builder/stages/selection/passthrough_selection_stage.h"
#include "sampic_event_builder/testing/crate_simulator.h"
#include "sampic_event_builder/testing/vector_input_stage.h"
#include "residual_compression/cpu_rice_golomb_codec.h"
#include "template_fitter/cpu_least_squares_fitter.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <vector>
#include <memory>

using namespace sampic::event_builder;

int main()
{
    CrateSimulator first({0, 256, 64, 100'000.0, 0.1, 10});
    CrateSimulator second({1, 256, 64, 100'000.0, 0.1, 20});
    const auto first_hits = first.generate(100);
    const auto second_hits = second.generate(100);
    assert(first_hits.size() == 100 && second_hits.size() == 100);
    assert(first_hits.front().crate == 0 && second_hits.front().crate == 1);

    std::vector<Hit> grouped_hits{
        Hit{0, 0, 0, 100, {1000}, false}, Hit{1, 0, 1, 150, {1000}, false},
        Hit{0, 0, 2, 400, {1000}, false}};
    const auto events = TimeWindowEventBuilder(200).build(grouped_hits);
    assert(events.size() == 2);
    assert(events[0].hits.size() == 2 && events[1].hits.size() == 1);

    TimeWindowEventBuilder held_builder({200, 800, 2});
    assert(held_builder.build({grouped_hits.data(), 1}).empty());
    assert(held_builder.build({grouped_hits.data() + 1, 1}).empty());
    const auto flushed = held_builder.flush();
    assert(flushed.size() == 1);
    assert(flushed.front().hits.size() == 2);

    TimeWindowEventBuilder zero_lateness_builder({100, 0, 2});
    assert(zero_lateness_builder.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{0, 0, 1, 250, {1000}, false},
    }).empty());
    const auto zero_lateness_events = zero_lateness_builder.build({
        Hit{1, 0, 2, 150, {1000}, false},
        Hit{1, 0, 3, 260, {1000}, false},
    });
    assert(zero_lateness_events.size() == 1);
    assert(zero_lateness_events.front().hits.size() == 2);

    FixedTimeBinEventBuilder fixed_bins({100, 0, 0, 2, false, 256});
    assert(fixed_bins.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{0, 0, 2, 250, {1000}, false},
    }).empty());
    auto fixed_events = fixed_bins.build({
        Hit{1, 0, 1, 150, {1000}, false},
        Hit{1, 0, 3, 260, {1000}, false},
    });
    assert(fixed_events.size() == 1);
    assert(fixed_events.front().start_ns == 100);
    assert(fixed_events.front().end_ns == 200);
    assert(fixed_events.front().hits.size() == 2);

    using Clock = PeriodicReadoutEventBuilder::Clock;
    auto now = Clock::time_point{};
    PeriodicReadoutEventBuilder periodic(
        {10'000, true}, [&now] { return now; });
    assert(periodic.build({
        Hit{0, 0, 0, 110, {1000}, false},
        Hit{1, 0, 1, 150, {1000}, false},
    }).empty());
    now += std::chrono::nanoseconds(10'000);
    const auto periodic_events = periodic.build({});
    assert(periodic_events.size() == 1);
    assert(periodic_events.front().hits.size() == 2);

    std::vector<double> shape(64, 0);
    for (std::size_t index = 16; index < shape.size(); ++index) shape[index] = index - 16;
    waveform::fitting::CpuLeastSquaresFitter fitter({
        waveform::fitting::CubicPulseTemplate(shape), 4.0, 0.125, 3.0});
    waveform::compression::CpuRiceGolombCodec codec(4);
    for (const bool fitting : {false, true}) {
        for (const bool compression : {false, true}) {
            std::unique_ptr<FittingStage> fitting_stage;
            if(fitting) fitting_stage=std::make_unique<TemplateFittingStage>(
                TemplateFittingStageConfig{42, 1.0e9},fitter);
            else fitting_stage=std::make_unique<PassthroughFittingStage>();
            std::unique_ptr<CompressionStage> compression_stage;
            if(compression) compression_stage=std::make_unique<ResidualCompressionStage>(codec);
            else compression_stage=std::make_unique<UncompressedStage>();
            EventBuilderPipeline pipeline(
                std::make_unique<testing::VectorInputStage>(std::vector<Hit>{first_hits.front()}),
                std::make_unique<TimeWindowEventBuilder>(200),
                std::make_unique<PassthroughSelectionStage>(),std::move(fitting_stage),
                std::move(compression_stage),std::make_unique<ProductBankFinalizationStage>());
            const auto finalized=pipeline.run_once(); assert(finalized.size()==1);
            const auto decoded=ProductBankDecoder{}.decode(finalized.front().product_bank);
            assert(decoded.size()==1); const auto& product=decoded.front();
            std::vector<std::int32_t> values;
            if (compression) {
                values = codec.decode({product.codec_id, product.sample_count,
                    product.payload_bit_count, product.payload});
            } else if (fitting) {
                values.resize(product.sample_count);
                std::memcpy(values.data(), product.payload.data(), product.payload.size());
            }
            std::vector<std::int16_t> rebuilt;
            if (fitting) {
                waveform::fitting::FitResult fit;
                fit.baseline=product.baseline; fit.amplitude_q=product.amplitude_q;
                fit.shift_q=product.shift_q; fit.chi_squared=product.chi_squared;
                fit.degrees_of_freedom=product.degrees_of_freedom;
                fit.residuals=std::move(values);
                rebuilt = fitter.reconstruct(fit);
                assert(product.template_id == 42);
            } else if (compression) {
                rebuilt.assign(values.begin(), values.end());
            } else {
                rebuilt.resize(product.sample_count);
                std::memcpy(rebuilt.data(), product.payload.data(), product.payload.size());
            }
            assert(rebuilt == first_hits.front().waveform);
        }
    }

    TopologyEventSelector selector({2, 1, 1'000'000});
    assert(selector.select(events[0]) == EventDecision::reject_through_going);
    assert(selector.select(events[1]) == EventDecision::keep);
}
