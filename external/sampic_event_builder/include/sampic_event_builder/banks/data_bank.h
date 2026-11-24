#pragma once
#include <cstdint>
#include <span>
#include "sampic_event_builder/model/event.h"
#include "processing/sampic_processing/collector/banks/sampic_hit_format.h"
namespace sampic::event_builder::banks {
class DataBank {
public:
    static constexpr const char* name="BD00";
    static constexpr std::uint32_t kProductBankMagic = 0x31424553; // SEB1
    static constexpr std::uint16_t kProductBankVersion = 4;
    // Version 4: each hit is crate + the exact compact SD and SH record layouts.
#pragma pack(push, 1)
    struct Header {
        std::uint32_t magic;
        std::uint16_t version, reserved;
        std::uint32_t waveform_count;
        std::uint64_t start_ns, end_ns;
    };
    struct Record {
        std::uint16_t crate;
        wire::CompactData data;
        wire::CompactAdvanced advanced;
    };
#pragma pack(pop)
    std::vector<std::uint8_t> encode(const Event& event) const;
    Event decode(std::span<const std::uint8_t> bank) const;
};
static_assert(sizeof(DataBank::Header)==28);
static_assert(sizeof(DataBank::Record)==188);
}
