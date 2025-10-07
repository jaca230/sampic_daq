#include "processing/sampic_processing/collector/banks/frontend_event_bank_collector_timing.h"

FrontendEventBankCollectorTiming::FrontendEventBankCollectorTiming(
    const Record& record,
    const std::string& prefix)
    : record_(record)
{
    bank_prefix_ = prefix;
}

const uint8_t* FrontendEventBankCollectorTiming::data() const {
    return reinterpret_cast<const uint8_t*>(&record_);
}

size_t FrontendEventBankCollectorTiming::size() const {
    return sizeof(Record);
}
