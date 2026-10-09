#ifndef FRONTEND_EVENT_BANK_ADVANCED_HITS_H
#define FRONTEND_EVENT_BANK_ADVANCED_HITS_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

#include <SAMPIC_256Ch_Type.h>

#include <cstddef>
#include <cstdint>
#include <vector>

/// Compact, stable serialization of the complete SAMPIC AdvancedParams data.
/// Record i corresponds to hit i in the event's regular data bank.
class FrontendEventBankAdvancedHits : public FrontendEventBank {
public:
    static constexpr std::uint16_t FORMAT_VERSION = 1;

#pragma pack(push, 1)
    struct Header {
        std::uint16_t format_version;
        std::uint16_t record_size;
        std::uint32_t hit_count;
    };

    struct Record {
        std::uint16_t sampic_data_header;
        std::int32_t first_trigger_position_cell;
        std::int32_t trigger_position_cell;
        double physical_cell0_timestamp_ns;
        std::int32_t sampic_timestamp_a;
        std::int32_t sampic_timestamp_b;
        std::uint64_t fpga_timestamp;
        std::int32_t adc_counter_latched_at_end_of_conversion;
        std::int32_t start_of_adc_ramp;
        std::uint64_t trigger_position_mask;
        std::int32_t time_physical_index;
    };
#pragma pack(pop)

    explicit FrontendEventBankAdvancedHits(const std::vector<const HitStruct*>& hits);

    const std::uint8_t* data() const override { return payload_.data(); }
    std::size_t size() const override { return payload_.size(); }

private:
    std::vector<std::uint8_t> payload_;
};

static_assert(sizeof(FrontendEventBankAdvancedHits::Header) == 8);
static_assert(sizeof(FrontendEventBankAdvancedHits::Record) == 54);

#endif
