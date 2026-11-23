#pragma once
#include <cstdint>
namespace sampic::event_builder::products {
inline constexpr std::uint32_t kProductBankMagic = 0x31424553; // "SEB1"
inline constexpr std::uint16_t kProductBankVersion = 3;
// Packed little-endian wire format. Version 3 intentionally rejects older products.
#pragma pack(push, 1)
struct ProductBankHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t reserved;
    std::uint32_t waveform_count;
    std::uint64_t start_ns, end_ns;
};
struct ProductWaveformHeader {
    std::uint16_t crate, board, channel;
    std::uint64_t timestamp_ns;
    std::uint32_t sample_count;
};
#pragma pack(pop)
static_assert(sizeof(ProductBankHeader) == 28);
static_assert(sizeof(ProductWaveformHeader) == 18);
}
