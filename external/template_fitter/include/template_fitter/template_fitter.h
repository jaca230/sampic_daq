#pragma once

#include "template_fitter/fit_result.h"

#include <cstdint>
#include <span>
#include <vector>

namespace waveform::fitting {

class TemplateFitter {
public:
    virtual ~TemplateFitter() = default;
    virtual FitResult fit(std::span<const std::int16_t> samples) const = 0;
    virtual std::vector<std::int16_t> reconstruct(const FitResult& fit) const = 0;
    virtual std::uint16_t algorithm_id() const = 0;
};

} // namespace waveform::fitting
