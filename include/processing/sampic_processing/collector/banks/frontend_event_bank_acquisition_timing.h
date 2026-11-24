#pragma once
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "integration/sampic/collector/sampic_event.h"
#include "sampic_event_builder/banks/acquisition_timing_bank.h"

// Exactly one vendor acquisition, without parent aggregation.
class FrontendEventBankAcquisitionTiming final : public FrontendEventBank {
public:
    FrontendEventBankAcquisitionTiming(const SampicEvent& parent,
                                      std::uint64_t acquisition_id, std::size_t hit_count);
    const std::uint8_t* data() const override {
        return reinterpret_cast<const std::uint8_t*>(&record_);
    }
    std::size_t size() const override { return sizeof(record_); }
private:
    sampic::event_builder::timing::Acquisition record_;
};
