#ifndef FRONTEND_EVENT_BANK_TIME_BIN_H
#define FRONTEND_EVENT_BANK_TIME_BIN_H
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

class FrontendEventBankTimeBin : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Record {
        std::uint16_t format_version = 1;
        std::uint16_t record_size = 32;
        std::uint32_t hit_count = 0;
        std::int64_t bin_index = 0;
        double start_ns = 0;
        double end_ns = 0; // Exclusive; hardware timestamp domain, not host wall time.
    };
#pragma pack(pop)
    explicit FrontendEventBankTimeBin(Record record) : record_(record) { setBankPrefix("SB"); }
    const std::uint8_t* data() const override { return reinterpret_cast<const std::uint8_t*>(&record_); }
    std::size_t size() const override { return sizeof(record_); }
private:
    Record record_;
};
static_assert(sizeof(FrontendEventBankTimeBin::Record) == 32);
#endif
