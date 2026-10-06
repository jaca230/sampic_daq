#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_CONFIG_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeVendorPassthroughConfig {
    std::uint32_t wait_timeout_ms = 1000;
    bool include_trigger_records = true;
    std::string data_bank_prefix = "AD";
    std::string event_timing_bank_prefix = "AT";
    std::string trigger_bank_prefix = "VT";
};

#endif
