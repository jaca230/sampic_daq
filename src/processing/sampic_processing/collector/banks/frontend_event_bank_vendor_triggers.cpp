#include "processing/sampic_processing/collector/banks/frontend_event_bank_vendor_triggers.h"

#include <algorithm>
#include <cstring>

FrontendEventBankVendorTriggers::FrontendEventBankVendorTriggers(
    const TriggerDataStruct& triggers) {
    setBankPrefix("SV");

    const int bounded_count = std::clamp(
        triggers.NbOfTriggers, 0, static_cast<int>(MAX_NB_OF_TRIGGERS_IN_EVENT));
    const Header header{static_cast<std::uint32_t>(bounded_count)};
    serialized_.resize(sizeof(Header) + sizeof(Record) * header.trigger_count);
    std::memcpy(serialized_.data(), &header, sizeof(header));

    for (int index = 0; index < bounded_count; ++index) {
        const Record record{
            static_cast<std::uint32_t>(triggers.TriggerIDFromFPGA[index]),
            triggers.TriggerIDFromExtTrig[index],
            triggers.SpillNumberFromExtTrig[index],
            triggers.RawExtraWord[index],
            triggers.TriggerTimeStamp[index],
        };
        std::memcpy(
            serialized_.data() + sizeof(Header) + sizeof(Record) * index,
            &record,
            sizeof(record));
    }
}

const std::uint8_t* FrontendEventBankVendorTriggers::data() const {
    return serialized_.data();
}

std::size_t FrontendEventBankVendorTriggers::size() const {
    return serialized_.size();
}
