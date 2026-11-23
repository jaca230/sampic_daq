#ifndef SAMPIC_DAQ_FRONTEND_EVENT_BANK_ORDERED_RAW_H
#define SAMPIC_DAQ_FRONTEND_EVENT_BANK_ORDERED_RAW_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "SAMPIC_256Ch_Type.h"

#include <cstdint>
#include <vector>

/// Versioned MIDAS bank containing time-ordered, pre-correction ADC samples.
///
/// Unlike FrontendEventBankData, this format does not expose slices of the
/// vendor HitStruct. Integer fields and samples are copied into a stable wire
/// representation so offline/event-builder readers do not depend on vendor
/// structure padding or on floating-point correction details.
class FrontendEventBankOrderedRaw final : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Header {
        std::uint32_t magic;
        std::uint16_t version;
        std::uint16_t header_size;
        std::uint32_t hit_count;
    };

    struct HitHeader {
        std::uint32_t record_size;
        std::int16_t fe_board_index;
        std::int16_t sampic_index;
        std::int16_t channel_index;
        std::int16_t channel;
        std::int32_t hit_number;
        std::int32_t cell_info;
        std::int32_t first_cell_physical_index;
        std::uint16_t sample_count;
        std::uint16_t reserved;
        double time_instant_ns;
        double first_cell_timestamp_ns;
    };
#pragma pack(pop)

    static constexpr std::uint32_t kMagic = 0x31575253U; // "SRW1"
    static constexpr std::uint16_t kVersion = 1;

    explicit FrontendEventBankOrderedRaw(
        const std::vector<const HitStruct*>& hits);

    const std::uint8_t* data() const override { return payload_.data(); }
    std::size_t size() const override { return payload_.size(); }

private:
    std::vector<std::uint8_t> payload_;
};

static_assert(sizeof(FrontendEventBankOrderedRaw::Header) == 12);
static_assert(sizeof(FrontendEventBankOrderedRaw::HitHeader) == 44);

#endif
