#pragma once

#include "sampic_event_builder/processing/processed_waveform.h"
#include "sampic_event_builder/products/event_product_format.h"
#include <span>
#include <vector>

namespace sampic::event_builder {

class ProductBankEncoder {
public:
    std::vector<std::uint8_t> encode(std::span<const ProcessedWaveform> waveforms) const;
};

} // namespace sampic::event_builder
