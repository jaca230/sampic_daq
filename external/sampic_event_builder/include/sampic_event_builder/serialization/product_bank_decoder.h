#pragma once

#include "sampic_event_builder/processing/processed_waveform.h"
#include <cstdint>
#include <span>
#include <vector>

namespace sampic::event_builder {

class ProductBankDecoder {
public:
    std::vector<ProcessedWaveform> decode(std::span<const std::uint8_t> bank) const;
};

} // namespace sampic::event_builder
