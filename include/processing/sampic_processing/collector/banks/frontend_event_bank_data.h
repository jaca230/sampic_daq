#ifndef FRONTEND_EVENT_BANK_DATA_H
#define FRONTEND_EVENT_BANK_DATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include <vector>
#include <memory>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

/// Bank representing waveform and scalar data from multiple HitStructs.
/// Keeps all parent EventStructs alive while exposing zero-copy slices.
class FrontendEventBankData : public FrontendEventBank {
public:
    FrontendEventBankData(std::vector<std::shared_ptr<const EventStruct>> parents,
                          const std::vector<const HitStruct*>& hits);

    const uint8_t* data() const override { return nullptr; }  // multi-slice bank
    size_t size() const override { return total_size_; }

    const std::vector<std::pair<const uint8_t*, size_t>>& slices() const { return slices_; }

private:
    std::vector<std::shared_ptr<const EventStruct>> parents_;  // keeps all parent data alive
    std::vector<std::pair<const uint8_t*, size_t>> slices_;
    size_t total_size_{0};
};

#endif // FRONTEND_EVENT_BANK_DATA_H
