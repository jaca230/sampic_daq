#ifndef SAMPIC_DAQ_FRONTEND_EVENT_BANK_VENDOR_TRIGGERS_H
#define SAMPIC_DAQ_FRONTEND_EVENT_BANK_VENDOR_TRIGGERS_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

#include <cstdint>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

/// Trigger records retained from one vendor EventStruct. This bank is used by
/// the vendor_passthrough mode, where a MIDAS event preserves the boundaries
/// of exactly one decoded SAMPIC event.
class FrontendEventBankVendorTriggers : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Header {
        std::uint32_t trigger_count;
    };

    struct Record {
        std::uint32_t fpga_trigger_id;
        std::uint32_t external_trigger_id;
        std::uint16_t spill_number;
        std::uint16_t raw_extra_word;
        double trigger_timestamp_ns;
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 4);
    static_assert(sizeof(Record) == 20);

    explicit FrontendEventBankVendorTriggers(const TriggerDataStruct& triggers);

    const std::uint8_t* data() const override;
    std::size_t size() const override;

private:
    std::vector<std::uint8_t> serialized_;
};

#endif
