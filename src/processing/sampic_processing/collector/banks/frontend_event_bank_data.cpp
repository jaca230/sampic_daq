#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include <cstddef>
#include <spdlog/spdlog.h>

/**
 * @brief Construct a data bank containing header + corrected waveform fields only.
 *
 * Excludes RawDataSamples[] and OrderedRawDataSamples[] to minimize event size.
 * Retains all header fields (e.g. indices, timestamps) and scalar timing data
 * after CorrectedDataSamples, up to but not including AdvancedParams.
 */
FrontendEventBankData::FrontendEventBankData(
    std::vector<std::shared_ptr<SampicEvent>> parents,
    const std::vector<const HitStruct*>& hits)
    : parents_(std::move(parents))
{
    bank_prefix_ = "AD";
    slices_.reserve(hits.size() * 2);  ///< header + corrected section
    total_size_ = 0;

    for (const HitStruct* h : hits) {
        if (!h) continue;

        // (1) Header: everything before RawDataSamples
        const size_t prefix_size = offsetof(HitStruct, RawDataSamples);
        slices_.emplace_back(reinterpret_cast<const uint8_t*>(h), prefix_size);

        // (2) Corrected waveform section: CorrectedDataSamples through end of scalars
        const size_t offset_corrected = offsetof(HitStruct, CorrectedDataSamples);
        const size_t offset_suffix    = offsetof(HitStruct, AdvancedParams);
        const size_t corrected_size   = offset_suffix - offset_corrected;

        slices_.emplace_back(reinterpret_cast<const uint8_t*>(h) + offset_corrected,
                             corrected_size);

        total_size_ += prefix_size + corrected_size;
    }

    spdlog::debug("FrontendEventBankData: built {} hits ({} bytes total, header + corrected only)",
                  hits.size(), total_size_);
}
