#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace waveform::fitting {

/// Baseline-subtracted pulse shape with Catmull-Rom interpolation between
/// aligned calibration samples.
class CubicPulseTemplate {
public:
    explicit CubicPulseTemplate(
        std::vector<double> samples, double samples_per_waveform_sample = 1.0);
    static CubicPulseTemplate from_aligned_waveforms(
        std::span<const std::vector<std::int16_t>> waveforms,
        std::size_t baseline_samples);

    [[nodiscard]] double evaluate(double sample_index) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }

private:
    std::vector<double> samples_;
    double sampling_density_ = 1.0;
};

} // namespace waveform::fitting
