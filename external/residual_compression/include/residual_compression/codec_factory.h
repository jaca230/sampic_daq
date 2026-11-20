#pragma once
#include "residual_compression/cpu_rice_golomb_codec.h"
#include <memory>
#include <string_view>

namespace waveform::compression {
class CodecFactory {
public:
    static std::unique_ptr<ResidualCodec> create(std::string_view type, std::uint8_t rice_k=4);
};
} // namespace waveform::compression
