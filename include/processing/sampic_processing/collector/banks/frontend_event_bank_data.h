#ifndef FRONTEND_EVENT_BANK_DATA_H
#define FRONTEND_EVENT_BANK_DATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "integration/sampic/collector/sampic_event.h"
#include <vector>
#include <memory>

/// Bank representing waveform and scalar data from multiple SampicEvents.
/// NOTE: Does not own the SampicEvents - caller must ensure they remain valid.
class FrontendEventBankData : public FrontendEventBank {
public:
    FrontendEventBankData(const std::vector<SampicEvent*>& parents,
                          const std::vector<const HitStruct*>& hits);

    const uint8_t* data() const override { return nullptr; }  // multi-slice
    size_t size() const override { return total_size_; }

    const std::vector<std::pair<const uint8_t*, size_t>>& slices() const { return slices_; }

private:
    std::vector<std::pair<const uint8_t*, size_t>> slices_;
    size_t total_size_{0};
};

#endif // FRONTEND_EVENT_BANK_DATA_H
