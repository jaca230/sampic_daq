#ifndef FRONTEND_PERIODIC_READOUT_CONFIG_H
#define FRONTEND_PERIODIC_READOUT_CONFIG_H
#include <cstdint>
#include <string>
struct FrontendCollectorModePeriodicReadoutConfig {
    std::uint64_t readout_period_ns = 10000;
    std::uint32_t max_pending_readouts = 1024;
    bool data_bank_enabled = true;
    std::string data_bank_prefix = "SD";
    bool advanced_bank_enabled = true;
    std::string advanced_bank_prefix = "SH";
    bool verbose_banks = false;
    std::string readout_bank_prefix = "SR";
    bool event_timing_bank_enabled = true;
    std::string event_timing_bank_prefix = "ST";
};
#endif
