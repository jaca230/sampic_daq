#pragma once

#include <cstdint>
#include <vector>

namespace sampic::event_builder {

enum ProcessingFlags : std::uint16_t {
    fitted = 1u << 0, compressed = 1u << 1, fit_rejected = 1u << 2
};

struct ProcessedWaveform {
    std::uint16_t crate = 0, board = 0, channel = 0;
    std::uint64_t timestamp_ns = 0;
    std::uint32_t sample_count = 0;
    std::uint16_t flags = 0, fitter_id = 0, codec_id = 0;
    std::uint64_t template_id = 0;
    std::int32_t baseline = 0, amplitude_q = 0;
    std::int16_t shift_q = 0;
    float chi_squared = 0.0F;
    std::uint16_t degrees_of_freedom = 0;
    std::uint32_t payload_bit_count = 0;
    std::vector<std::uint8_t> payload;
};

} // namespace sampic::event_builder
