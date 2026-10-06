#include "processing/sampic_processing/collector/banks/frontend_event_bank_trigger_metadata.h"

FrontendEventBankTriggerMetadata::FrontendEventBankTriggerMetadata(Record record)
    : record_(record) {
    setBankPrefix("TG");
}

const uint8_t* FrontendEventBankTriggerMetadata::data() const {
    return reinterpret_cast<const uint8_t*>(&record_);
}

size_t FrontendEventBankTriggerMetadata::size() const {
    return sizeof(record_);
}

const FrontendEventBankTriggerMetadata::Record&
FrontendEventBankTriggerMetadata::record() const {
    return record_;
}
