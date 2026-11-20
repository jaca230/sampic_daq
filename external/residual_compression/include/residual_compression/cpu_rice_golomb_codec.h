#pragma once

#include "residual_compression/residual_codec.h"

namespace waveform::compression {

class CpuRiceGolombCodec final : public ResidualCodec {
public:
    static constexpr std::uint16_t kCodecId = 1;
    explicit CpuRiceGolombCodec(std::uint8_t rice_k = 4);
    EncodedBlock encode(std::span<const std::int32_t> values) const override;
    std::vector<std::int32_t> decode(const EncodedBlock& block) const override;
    std::uint16_t codec_id() const override { return kCodecId; }
private:
    std::uint8_t rice_k_;
};

} // namespace waveform::compression
