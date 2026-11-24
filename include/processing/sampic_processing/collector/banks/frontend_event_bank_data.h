#ifndef FRONTEND_EVENT_BANK_DATA_H
#define FRONTEND_EVENT_BANK_DATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include <SAMPIC_256Ch_Type.h>
#include <vector>
#include "processing/sampic_processing/collector/banks/sampic_hit_format.h"

// Versioned, packed SD format. Integers/floats use the host's little-endian wire order.
class FrontendEventBankData : public FrontendEventBank {
public:
    static constexpr std::uint16_t FORMAT_VERSION = 1;
    enum Flags : std::uint8_t {
        INL_CORRECTED = 1 << 0,
        ADC_CORRECTED = 1 << 1,
        RESIDUAL_PEDESTAL_CORRECTED = 1 << 2,
        WAVEFORM_CLIPPED = 1 << 3,
        WAVEFORM_NONFINITE = 1 << 4,
        RAW_TOT_CLIPPED = 1 << 5,
    };
#pragma pack(push, 1)
    struct Header {
        char magic[4]; // "SDC1", distinguishes this format from historical unversioned SD.
        std::uint16_t format_version;
        std::uint16_t record_size;
        std::uint32_t hit_count;
    };
    using Record = sampic::wire::CompactData;
#pragma pack(pop)
    explicit FrontendEventBankData(const std::vector<const HitStruct*>& hits);
    const std::uint8_t* data() const override { return payload_.data(); }
    std::size_t size() const override { return payload_.size(); }
private:
    std::vector<std::uint8_t> payload_;
};
static_assert(sizeof(FrontendEventBankData::Header) == 12);
static_assert(sizeof(FrontendEventBankData::Record) == 154);
#endif
