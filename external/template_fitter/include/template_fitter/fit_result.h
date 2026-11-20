#pragma once

#include <cstdint>
#include <vector>

namespace waveform::fitting {

struct FitResult {
    static constexpr double kShiftScale = 256.0;
    std::int32_t baseline = 0;
    std::int32_t amplitude_q = 0;
    std::int16_t shift_q = 0;
    double chi_squared = 0.0;
    std::uint16_t degrees_of_freedom = 0;
    std::vector<std::int32_t> residuals;

    [[nodiscard]] double shift_samples() const noexcept {
        return static_cast<double>(shift_q) / kShiftScale;
    }
};

} // namespace waveform::fitting
