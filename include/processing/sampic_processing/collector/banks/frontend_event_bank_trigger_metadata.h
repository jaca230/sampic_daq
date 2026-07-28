#ifndef FRONTEND_EVENT_BANK_TRIGGER_METADATA_H
#define FRONTEND_EVENT_BANK_TRIGGER_METADATA_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

#include <cstdint>

/// Compact event-level provenance for a decoded external-trigger record.
class FrontendEventBankTriggerMetadata : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Record {
        uint32_t fpga_trigger_id;
        uint32_t external_trigger_id;
        uint32_t trigger_index_in_sampic_event;
        uint32_t assigned_hits;
        uint32_t ambiguous_hits;
        double trigger_timestamp_ns;
        double hit_reference_timestamp_ns;
    };
#pragma pack(pop)

    explicit FrontendEventBankTriggerMetadata(Record record);

    const uint8_t* data() const override;
    size_t size() const override;
    const Record& record() const;

private:
    Record record_{};
};

#endif
