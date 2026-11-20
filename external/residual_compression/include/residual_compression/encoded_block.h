#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace waveform::compression {

struct EncodedBlock {
    std::uint16_t codec_id = 0;
    std::uint32_t value_count = 0;
    std::uint32_t bit_count = 0;
    std::vector<std::uint8_t> bytes;
    [[nodiscard]] std::size_t encoded_bytes() const { return bytes.size(); }
};

} // namespace waveform::compression
