#ifndef FRONTEND_EVENT_BANK_DATA_H
#define FRONTEND_EVENT_BANK_DATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "integration/sampic/collector/sampic_event.h"
#include <vector>
#include <memory>

/// Bank representing waveform and scalar data from multiple SampicEvents.
/// Keeps SampicEvents alive and exposes zero-copy slices of their HitStructs.
class FrontendEventBankData : public FrontendEventBank {
public:
    FrontendEventBankData(std::vector<std::shared_ptr<SampicEvent>> parents,
                          const std::vector<const HitStruct*>& hits);

    const uint8_t* data() const override { return nullptr; }  // multi-slice
    size_t size() const override { return total_size_; }

    const std::vector<std::pair<const uint8_t*, size_t>>& slices() const { return slices_; }

private:
    std::vector<std::shared_ptr<SampicEvent>> parents_;  ///< Keep SampicEvents alive
    std::vector<std::pair<const uint8_t*, size_t>> slices_;
    size_t total_size_{0};
};

#endif // FRONTEND_EVENT_BANK_DATA_H
