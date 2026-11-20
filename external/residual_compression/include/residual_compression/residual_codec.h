#pragma once

#include "residual_compression/encoded_block.h"
#include <cstdint>
#include <span>
#include <vector>

namespace waveform::compression {

class ResidualCodec {
public:
    virtual ~ResidualCodec() = default;
    virtual EncodedBlock encode(std::span<const std::int32_t> values) const = 0;
    virtual std::vector<std::int32_t> decode(const EncodedBlock& block) const = 0;
    virtual std::uint16_t codec_id() const = 0;
};

} // namespace waveform::compression
