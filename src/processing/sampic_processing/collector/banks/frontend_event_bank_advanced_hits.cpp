#include "processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

FrontendEventBankAdvancedHits::FrontendEventBankAdvancedHits(
    const std::vector<const HitStruct*>& hits) {
    setBankPrefix("SH");

    const auto hit_count = std::count_if(
        hits.begin(), hits.end(), [](const HitStruct* hit) { return hit != nullptr; });
    if (hit_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("too many hits for compact advanced-hit bank");
    }

    const Header header{
        FORMAT_VERSION,
        static_cast<std::uint16_t>(sizeof(Record)),
        static_cast<std::uint32_t>(hit_count),
    };
    payload_.resize(sizeof(Header) + hit_count * sizeof(Record));
    std::memcpy(payload_.data(), &header, sizeof(header));

    auto* destination = payload_.data() + sizeof(Header);
    for (const HitStruct* hit : hits) {
        if (!hit) {
            continue;
        }

        std::uint64_t trigger_position_mask = 0;
        for (int sample = 0; sample < 64; ++sample) {
            if (hit->AdvancedParams.TriggerPosition[sample] != FALSE) {
                trigger_position_mask |= (std::uint64_t{1} << sample);
            }
        }

        const Record record{
            hit->AdvancedParams.SampicDataHeader,
            hit->AdvancedParams.FirstTriggerPositionCell,
            hit->AdvancedParams.TriggerPositionCell,
            hit->AdvancedParams.PhysicalCell0TimeStamp,
            hit->AdvancedParams.SampicTimeStampA,
            hit->AdvancedParams.SampicTimeStampB,
            static_cast<std::uint64_t>(hit->AdvancedParams.FPGATimeStamp),
            hit->AdvancedParams.ADCCounter_LatchedAtEndOfConv,
            hit->AdvancedParams.StartOfADCRamp,
            trigger_position_mask,
            hit->AdvancedParams.TimePhysicalIndex,
        };
        std::memcpy(destination, &record, sizeof(record));
        destination += sizeof(record);
    }
}
