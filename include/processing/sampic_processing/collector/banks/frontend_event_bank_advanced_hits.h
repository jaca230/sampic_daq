#ifndef FRONTEND_EVENT_BANK_ADVANCED_HITS_H
#define FRONTEND_EVENT_BANK_ADVANCED_HITS_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

#include <SAMPIC_256Ch_Type.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include "processing/sampic_processing/collector/banks/sampic_hit_format.h"

/// Optimized maximum timing metadata (32 bytes/hit).
/// Record i corresponds to hit i in the event's regular data bank.
class FrontendEventBankAdvancedHits : public FrontendEventBank {
public:
    static constexpr std::uint16_t FORMAT_VERSION = 2;

#pragma pack(push, 1)
    struct Header {
        std::uint16_t format_version;
        std::uint16_t record_size;
        std::uint32_t hit_count;
    };

    using Record = sampic::wire::CompactAdvanced;
#pragma pack(pop)

    explicit FrontendEventBankAdvancedHits(const std::vector<const HitStruct*>& hits);

    const std::uint8_t* data() const override { return payload_.data(); }
    std::size_t size() const override { return payload_.size(); }

private:
    std::vector<std::uint8_t> payload_;
};

static_assert(sizeof(FrontendEventBankAdvancedHits::Header) == 8);
static_assert(sizeof(FrontendEventBankAdvancedHits::Record) == 32);

#endif
