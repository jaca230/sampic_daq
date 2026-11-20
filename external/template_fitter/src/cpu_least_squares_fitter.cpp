#include "template_fitter/cpu_least_squares_fitter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace waveform::fitting {
namespace { constexpr std::int32_t kAmplitudeScale = 1024; }

CpuLeastSquaresFitter::CpuLeastSquaresFitter(CpuLeastSquaresConfig config)
    : config_(std::move(config)) {
    if (config_.shift_step <= 0.0 || config_.maximum_shift < 0.0 ||
        config_.noise_sigma <= 0.0) {
        throw std::invalid_argument("fit shift grid and noise sigma must be positive");
    }
}

FitResult CpuLeastSquaresFitter::fit(std::span<const std::int16_t> samples) const {
    if (samples.size() < 4) throw std::invalid_argument("waveform needs at least four samples");

    double best_error = std::numeric_limits<double>::infinity();
    double best_amplitude = 0.0, best_baseline = 0.0, best_shift = 0.0;
    const auto steps = static_cast<int>(std::ceil(
        2.0 * config_.maximum_shift / config_.shift_step));
    for (int step = 0; step <= steps; ++step) {
        const double shift = -config_.maximum_shift + step * config_.shift_step;
        double sum_x = 0.0, sum_y = 0.0, sum_xx = 0.0, sum_xy = 0.0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double x = config_.pulse_template.evaluate(static_cast<double>(i) - shift);
            const double y = samples[i];
            sum_x += x; sum_y += y; sum_xx += x * x; sum_xy += x * y;
        }
        const double n = static_cast<double>(samples.size());
        const double determinant = n * sum_xx - sum_x * sum_x;
        if (std::abs(determinant) < 1e-12) continue;
        const double amplitude = (n * sum_xy - sum_x * sum_y) / determinant;
        const double baseline = (sum_y - amplitude * sum_x) / n;
        double error = 0.0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double residual = samples[i] - baseline - amplitude *
                config_.pulse_template.evaluate(static_cast<double>(i) - shift);
            error += residual * residual;
        }
        if (error < best_error) {
            best_error = error; best_amplitude = amplitude;
            best_baseline = baseline; best_shift = shift;
        }
    }
    if (!std::isfinite(best_error)) throw std::runtime_error("template fit is singular");

    FitResult result;
    result.baseline = static_cast<std::int32_t>(std::llround(best_baseline));
    result.amplitude_q = static_cast<std::int32_t>(std::llround(best_amplitude * kAmplitudeScale));
    result.shift_q = static_cast<std::int16_t>(std::clamp<long long>(
        std::llround(best_shift * FitResult::kShiftScale),
        std::numeric_limits<std::int16_t>::min(),
        std::numeric_limits<std::int16_t>::max()));
    result.degrees_of_freedom = static_cast<std::uint16_t>(
        std::min<std::size_t>(samples.size() - 3,
                              std::numeric_limits<std::uint16_t>::max()));
    result.residuals.reserve(samples.size());
    double quantized_error = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto residual = static_cast<std::int32_t>(samples[i]) -
            prediction(i, result.baseline, result.amplitude_q, result.shift_q);
        result.residuals.push_back(residual);
        quantized_error += static_cast<double>(residual) * residual;
    }
    result.chi_squared = quantized_error / (config_.noise_sigma * config_.noise_sigma);
    return result;
}

std::vector<std::int16_t> CpuLeastSquaresFitter::reconstruct(const FitResult& fit) const {
    std::vector<std::int16_t> samples;
    samples.reserve(fit.residuals.size());
    for (std::size_t i = 0; i < fit.residuals.size(); ++i) {
        const auto value = prediction(i, fit.baseline, fit.amplitude_q, fit.shift_q) + fit.residuals[i];
        if (value < std::numeric_limits<std::int16_t>::min() ||
            value > std::numeric_limits<std::int16_t>::max()) {
            throw std::runtime_error("reconstructed sample outside int16 range");
        }
        samples.push_back(static_cast<std::int16_t>(value));
    }
    return samples;
}

std::int32_t CpuLeastSquaresFitter::prediction(
    std::size_t sample, std::int32_t baseline,
    std::int32_t amplitude_q, std::int16_t shift_q) const {
    const double shifted = static_cast<double>(sample) -
        static_cast<double>(shift_q) / FitResult::kShiftScale;
    return baseline + static_cast<std::int32_t>(std::llround(
        static_cast<double>(amplitude_q) * config_.pulse_template.evaluate(shifted) /
        kAmplitudeScale));
}

} // namespace waveform::fitting
