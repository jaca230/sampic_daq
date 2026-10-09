#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_CONFIG_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeVendorPassthroughConfig {
    std::uint32_t wait_timeout_ms = 1000;
    bool data_bank_enabled = true;
    std::string data_bank_prefix = "SD";
    bool advanced_bank_enabled = true;
    std::string advanced_bank_prefix = "SH";
    bool event_timing_bank_enabled = true;
    std::string event_timing_bank_prefix = "ST";
    bool trigger_bank_enabled = true;
    std::string trigger_bank_prefix = "SV";
};

#endif
