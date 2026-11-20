#include "template_fitter/fitter_factory.h"
#include <stdexcept>
#include <string>

namespace waveform::fitting {
std::unique_ptr<TemplateFitter> FitterFactory::create(
    std::string_view type, CpuLeastSquaresConfig config) {
    if(type=="cpu-least-squares") return std::make_unique<CpuLeastSquaresFitter>(std::move(config));
    throw std::invalid_argument("unknown template fitter: "+std::string(type));
}
} // namespace waveform::fitting
