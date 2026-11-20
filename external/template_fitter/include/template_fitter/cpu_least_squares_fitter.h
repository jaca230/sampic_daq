#pragma once

#include "template_fitter/template_fitter.h"
#include "template_fitter/cubic_pulse_template.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace waveform::fitting {

struct CpuLeastSquaresConfig {
    CubicPulseTemplate pulse_template;
    double maximum_shift = 4.0;
    double shift_step = 0.125;
    double noise_sigma = 3.0;
};

class CpuLeastSquaresFitter final : public TemplateFitter {
public:
    static constexpr std::uint16_t kAlgorithmId = 1;
    explicit CpuLeastSquaresFitter(CpuLeastSquaresConfig config);
    FitResult fit(std::span<const std::int16_t> samples) const override;
    std::vector<std::int16_t> reconstruct(const FitResult& fit) const override;
    std::uint16_t algorithm_id() const override { return kAlgorithmId; }

private:
    std::int32_t prediction(std::size_t sample, std::int32_t baseline,
                            std::int32_t amplitude_q, std::int16_t shift_q) const;
    CpuLeastSquaresConfig config_;
};

} // namespace waveform::fitting
