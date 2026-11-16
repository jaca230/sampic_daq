#ifndef FRONTEND_EVENT_BANK_READOUT_TICK_H
#define FRONTEND_EVENT_BANK_READOUT_TICK_H
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
class FrontendEventBankReadoutTick : public FrontendEventBank {
public:
    enum Flags : std::uint32_t { LATE = 1, COALESCED = 2, FINAL_READOUT = 4 };
#pragma pack(push, 1)
    struct Record {
        std::uint16_t format_version = 1;
        std::uint16_t record_size = 56;
        std::uint32_t flags = 0;
        std::uint64_t first_tick = 0;
        std::uint64_t ticks_elapsed = 0;
        // Host steady-clock nanoseconds relative to collection start, not hardware time.
        std::uint64_t start_ns = 0;
        std::uint64_t end_ns = 0;
        std::uint64_t actual_readout_ns = 0;
        std::uint32_t hit_count = 0;
        std::uint32_t parent_count = 0;
    };
#pragma pack(pop)
    explicit FrontendEventBankReadoutTick(Record record) : record_(record) { setBankPrefix("SR"); }
    const std::uint8_t* data() const override { return reinterpret_cast<const std::uint8_t*>(&record_); }
    std::size_t size() const override { return sizeof(record_); }
private:
    Record record_;
};
static_assert(sizeof(FrontendEventBankReadoutTick::Record) == 56);
#endif
