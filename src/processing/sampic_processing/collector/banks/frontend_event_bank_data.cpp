#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include <cstddef>

FrontendEventBankData::FrontendEventBankData(
    std::vector<std::shared_ptr<const EventStruct>> parents,
    const std::vector<const HitStruct*>& hits)
    : parents_(std::move(parents)) {

    setBankPrefix("AD");
    slices_.reserve(hits.size() * 2);

    for (const HitStruct* h : hits) {
        if (!h) continue;

        // Prefix slice: everything before RawDataSamples[]
        size_t prefix_size = offsetof(HitStruct, RawDataSamples);
        slices_.emplace_back(reinterpret_cast<const uint8_t*>(h), prefix_size);

        // Suffix slice: after CorrectedDataSamples[]
        size_t offset_after_corrected =
            offsetof(HitStruct, CorrectedDataSamples) + sizeof(h->CorrectedDataSamples);
        size_t offset_suffix = offsetof(HitStruct, AdvancedParams);
        size_t suffix_size = offset_suffix - offset_after_corrected;

        slices_.emplace_back(reinterpret_cast<const uint8_t*>(h) + offset_after_corrected, suffix_size);
        total_size_ += prefix_size + suffix_size;
    }
}
