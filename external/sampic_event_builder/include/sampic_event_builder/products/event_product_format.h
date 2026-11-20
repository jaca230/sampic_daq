#pragma once

#include <cstdint>

namespace sampic::event_builder::products {

inline constexpr std::uint32_t kProductBankMagic = 0x31424553; // "SEB1" little-endian
inline constexpr std::uint16_t kProductBankVersion = 2;

#pragma pack(push, 1)
struct ProductBankHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t reserved;
    std::uint32_t waveform_count;
};

struct ProductWaveformHeader {
    std::uint16_t crate, board, channel, flags, fitter_id, codec_id;
    std::uint64_t timestamp_ns, template_id;
    std::uint32_t sample_count;
    std::int32_t baseline, amplitude_q;
    std::int16_t shift_q;
    float chi_squared;
    std::uint16_t degrees_of_freedom;
    std::uint16_t reserved;
    std::uint32_t payload_size, payload_bit_count;
};
#pragma pack(pop)

static_assert(sizeof(ProductBankHeader) == 12);
static_assert(sizeof(ProductWaveformHeader) == 58);

} // namespace sampic::event_builder::products
