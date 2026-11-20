#include "template_fitter/cubic_pulse_template.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace waveform::fitting {

CubicPulseTemplate::CubicPulseTemplate(
    std::vector<double> samples, double samples_per_waveform_sample)
    : samples_(std::move(samples)), sampling_density_(samples_per_waveform_sample) {
    if (samples_.size() < 4 || sampling_density_ <= 0.0) {
        throw std::invalid_argument("a cubic pulse template needs at least four samples");
    }
}

CubicPulseTemplate CubicPulseTemplate::from_aligned_waveforms(
    std::span<const std::vector<std::int16_t>> waveforms,
    std::size_t baseline_samples) {
    if (waveforms.empty() || waveforms.front().size() < 4) {
        throw std::invalid_argument("aligned template training set is empty");
    }
    const auto count = waveforms.front().size();
    std::vector<double> mean(count, 0.0);
    for (const auto& waveform : waveforms) {
        if (waveform.size() != count) {
            throw std::invalid_argument("aligned template waveforms differ in length");
        }
        const auto pedestal_count = std::min(baseline_samples, count);
        double pedestal = 0.0;
        for (std::size_t i = 0; i < pedestal_count; ++i) pedestal += waveform[i];
        pedestal /= static_cast<double>(pedestal_count);
        for (std::size_t i = 0; i < count; ++i) mean[i] += waveform[i] - pedestal;
    }
    for (auto& value : mean) value /= static_cast<double>(waveforms.size());
    const auto peak = std::max_element(mean.begin(), mean.end(),
        [](double left, double right) { return std::abs(left) < std::abs(right); });
    if (peak == mean.end() || std::abs(*peak) < 1e-12) {
        throw std::invalid_argument("aligned template has zero amplitude");
    }
    const double scale = *peak;
    for (auto& value : mean) value /= scale;
    return CubicPulseTemplate(std::move(mean));
}

double CubicPulseTemplate::evaluate(double index) const noexcept {
    index *= sampling_density_;
    if (index < 0.0 || index > static_cast<double>(samples_.size() - 1)) return 0.0;
    const auto i1 = static_cast<std::size_t>(std::floor(index));
    const auto at = [this](std::ptrdiff_t i) {
        i = std::clamp<std::ptrdiff_t>(i, 0, samples_.size() - 1);
        return samples_[static_cast<std::size_t>(i)];
    };
    const double fraction = index - static_cast<double>(i1);
    const double p0 = at(static_cast<std::ptrdiff_t>(i1) - 1);
    const double p1 = at(static_cast<std::ptrdiff_t>(i1));
    const double p2 = at(static_cast<std::ptrdiff_t>(i1) + 1);
    const double p3 = at(static_cast<std::ptrdiff_t>(i1) + 2);
    return 0.5 * ((2.0 * p1) + (-p0 + p2) * fraction +
        (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * fraction * fraction +
        (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * fraction * fraction * fraction);
}

} // namespace waveform::fitting
