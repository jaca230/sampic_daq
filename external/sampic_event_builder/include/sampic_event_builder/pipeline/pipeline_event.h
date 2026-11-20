#pragma once

#include "sampic_event_builder/model/event.h"
#include "sampic_event_builder/processing/processed_waveform.h"

#include <cstdint>
#include <vector>

namespace sampic::event_builder {

struct PipelineWaveform {
    ProcessedWaveform product;
    std::vector<std::int32_t> values;
};

struct PipelineEvent {
    Event source;
    std::vector<PipelineWaveform> waveforms;
};

struct FinalizedEvent {
    std::uint64_t start_ns = 0;
    std::uint64_t end_ns = 0;
    std::vector<std::uint8_t> product_bank;
};

} // namespace sampic::event_builder
