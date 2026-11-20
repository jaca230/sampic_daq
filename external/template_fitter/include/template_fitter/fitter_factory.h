#pragma once
#include "template_fitter/cpu_least_squares_fitter.h"
#include <memory>
#include <string_view>

namespace waveform::fitting {
class FitterFactory {
public:
    static std::unique_ptr<TemplateFitter> create(
        std::string_view type, CpuLeastSquaresConfig config);
};
} // namespace waveform::fitting
