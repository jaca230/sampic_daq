#include "template_fitter/cpu_least_squares_fitter.h"
#include <cassert>
#include <vector>

int main() {
    std::vector<double> shape(32);
    for (std::size_t i = 8; i < shape.size(); ++i) shape[i] = static_cast<double>(i - 8);
    waveform::fitting::CpuLeastSquaresFitter fitter({
        waveform::fitting::CubicPulseTemplate(shape), 3.0, 0.125, 1.0});
    std::vector<std::int16_t> samples(32, 1000);
    for (std::size_t i = 8; i < samples.size(); ++i) samples[i] += static_cast<std::int16_t>(2 * (i - 8));
    const auto fit = fitter.fit(samples);
    assert(fitter.reconstruct(fit) == samples);
    assert(fit.degrees_of_freedom == 29);
    assert(fit.chi_squared < 1.0);
}
