#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_RAW_PASSTHROUGH_CONFIG_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_RAW_PASSTHROUGH_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeVendorRawPassthroughConfig {
    std::uint32_t wait_timeout_ms = 1000;
    bool raw_data_bank_enabled = true;
    std::string raw_data_bank_prefix = "SR";
    bool event_timing_bank_enabled = true;
    std::string event_timing_bank_prefix = "ST";
    bool trigger_bank_enabled = true;
    std::string trigger_bank_prefix = "SV";
};

#endif
