#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "sampic_event_builder/metrics/stage_metrics.h"
#include "sampic_event_builder/stages/compression/residual_compression_stage.h"
#include "sampic_event_builder/stages/compression/uncompressed_stage.h"
#include "sampic_event_builder/stages/fitting/passthrough_fitting_stage.h"
#include "sampic_event_builder/stages/fitting/template_fitting_stage.h"
#include "sampic_event_builder/testing/crate_simulator.h"
#include "residual_compression/cpu_rice_golomb_codec.h"
#include "template_fitter/cpu_least_squares_fitter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

using namespace sampic::event_builder;

namespace {
std::size_t argument(int argc, char** argv, std::string_view name, std::size_t fallback) {
    for (int index = 1; index + 1 < argc; ++index) if (argv[index] == name) return std::stoull(argv[index + 1]);
    return fallback;
}
bool enabled_argument(int argc, char** argv, std::string_view name, bool fallback) {
    return argument(argc, argv, name, fallback ? 1 : 0) != 0;
}
waveform::fitting::CubicPulseTemplate pulse_template(std::size_t samples) {
    constexpr std::size_t density = 16;
    std::vector<double> result(samples * density);
    for (std::size_t index = 0; index < result.size(); ++index) {
        const double t = static_cast<double>(index) / density - 0.30 * samples;
        result[index] = t > 0.0
            ? std::exp(-t / 18.0) - std::exp(-t / 2.5) : 0.0;
    }
    return waveform::fitting::CubicPulseTemplate(std::move(result), density);
}
}

int main(int argc, char** argv)
{
    const auto crates = argument(argc, argv, "--crates", 2);
    const auto hits_per_crate = argument(argc, argv, "--hits", 100'000);
    const auto samples = argument(argc, argv, "--samples", 64);
    const auto window_ns = argument(argc, argv, "--window-ns", 200);
    const bool fitting_enabled = enabled_argument(argc, argv, "--fit", true);
    const bool compression_enabled = enabled_argument(argc, argv, "--compress", true);
    StageMetrics metrics;

    auto start = std::chrono::steady_clock::now();
    std::vector<Hit> hits;
    hits.reserve(crates * hits_per_crate);
    for (std::size_t crate = 0; crate < crates; ++crate) {
        CrateSimulator simulator({static_cast<std::uint16_t>(crate), 256,
            static_cast<std::uint32_t>(samples), 100'000.0, 0.10, crate + 1});
        auto generated = simulator.generate(hits_per_crate);
        std::move(generated.begin(), generated.end(), std::back_inserter(hits));
    }
    std::ranges::sort(hits, {}, &Hit::timestamp_ns);
    auto elapsed = std::chrono::steady_clock::now() - start;
    const std::uint64_t raw_bytes = hits.size() * samples * sizeof(std::int16_t);
    metrics.add("simulate+merge", hits.size(), raw_bytes, elapsed);

    start = std::chrono::steady_clock::now();
    const auto events = TimeWindowEventBuilder(window_ns).build(hits);
    elapsed = std::chrono::steady_clock::now() - start;
    metrics.add("event-build", hits.size(), raw_bytes, elapsed);

    start = std::chrono::steady_clock::now();
    const TopologyEventSelector selector({8, 32, 3'000});
    std::size_t kept = 0;
    std::size_t true_positive = 0;
    std::size_t false_positive = 0;
    std::size_t false_negative = 0;
    for (const auto& event : events) {
        const bool truth = std::ranges::any_of(
            event.hits, [](const Hit& hit) { return hit.simulated_physics; });
        const bool accepted = selector.select(event) == EventDecision::keep;
        kept += accepted;
        true_positive += truth && accepted;
        false_positive += !truth && accepted;
        false_negative += truth && !accepted;
    }
    elapsed = std::chrono::steady_clock::now() - start;
    metrics.add("event-select", events.size(), raw_bytes, elapsed);

    start = std::chrono::steady_clock::now();
    const waveform::fitting::CpuLeastSquaresFitter fitter({
        pulse_template(samples), 4.0, 0.125, 3.0});
    const waveform::compression::CpuRiceGolombCodec codec(4);
    std::unique_ptr<FittingStage> fitting = fitting_enabled
        ? std::unique_ptr<FittingStage>(std::make_unique<TemplateFittingStage>(
              TemplateFittingStageConfig{1, 5.0}, fitter))
        : std::unique_ptr<FittingStage>(std::make_unique<PassthroughFittingStage>());
    std::unique_ptr<CompressionStage> compression = compression_enabled
        ? std::unique_ptr<CompressionStage>(std::make_unique<ResidualCompressionStage>(codec))
        : std::unique_ptr<CompressionStage>(std::make_unique<UncompressedStage>());
    std::uint64_t compressed_bytes = 0;
    for (const auto& hit : hits) {
        PipelineEvent pipeline_event{Event{hit.timestamp_ns,hit.timestamp_ns,{hit}}, {}};
        fitting->fit(pipeline_event); compression->compress(pipeline_event);
        const auto& product = pipeline_event.waveforms.front().product;
        std::vector<std::int32_t> values;
        const bool product_compressed =
            (product.flags & ProcessingFlags::compressed) != 0;
        const bool product_fitted =
            (product.flags & ProcessingFlags::fitted) != 0;
        if (product_compressed) {
            values = codec.decode({product.codec_id, product.sample_count,
                product.payload_bit_count, product.payload});
        } else if (product_fitted) {
            values.resize(product.sample_count);
            std::memcpy(values.data(), product.payload.data(), product.payload.size());
        }
        std::vector<std::int16_t> reconstructed;
        if (product_fitted) {
            waveform::fitting::FitResult fit;
            fit.baseline=product.baseline; fit.amplitude_q=product.amplitude_q;
            fit.shift_q=product.shift_q; fit.chi_squared=product.chi_squared;
            fit.degrees_of_freedom=product.degrees_of_freedom;
            fit.residuals=std::move(values);
            reconstructed = fitter.reconstruct(fit);
        } else if (product_compressed) {
            reconstructed.assign(values.begin(), values.end());
        } else {
            reconstructed.resize(product.sample_count);
            std::memcpy(reconstructed.data(), product.payload.data(), product.payload.size());
        }
        if (reconstructed != hit.waveform) {
            std::cerr << "lossless round-trip failed\n";
            return EXIT_FAILURE;
        }
        compressed_bytes += product.payload.size() + 48;
    }
    elapsed = std::chrono::steady_clock::now() - start;
    metrics.add("process+verify", hits.size(), raw_bytes, elapsed);

    std::cout << "crates=" << crates << " hits=" << hits.size() << " events=" << events.size()
              << " kept=" << kept << " rejected=" << events.size() - kept << '\n';
    std::cout << "selector_tp=" << true_positive << " selector_fp=" << false_positive
              << " selector_fn=" << false_negative << '\n';
    std::cout << "fit=" << fitting_enabled << " compress=" << compression_enabled << '\n';
    std::cout << "raw_bytes=" << raw_bytes << " product_bytes=" << compressed_bytes
              << " ratio=" << std::fixed << std::setprecision(3)
              << static_cast<double>(raw_bytes) / compressed_bytes << "x\n\n";
    std::cout << std::left << std::setw(20) << "stage" << std::right << std::setw(16) << "items/s"
              << std::setw(16) << "MB/s" << std::setw(16) << "elapsed ms" << '\n';
    for (const auto& measurement : metrics.measurements()) {
        std::cout << std::left << std::setw(20) << measurement.name << std::right << std::fixed
                  << std::setprecision(1) << std::setw(16) << measurement.items_per_second()
                  << std::setw(16) << measurement.megabytes_per_second()
                  << std::setw(16) << measurement.elapsed.count() / 1.0e6 << '\n';
    }
}
