#ifndef FRONTEND_COLLECTOR_MODE_TIME_GROUPING_CONFIG_H
#define FRONTEND_COLLECTOR_MODE_TIME_GROUPING_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeTimeGroupingConfig {
    double time_window_ns = 1000000.0;
    double finalize_after_ms = 10.0;
    std::uint32_t wait_timeout_ms = 1000;
    bool data_bank_enabled = true;
    std::string data_bank_prefix = "SD";
    bool event_timing_bank_enabled = true;
    std::string event_timing_bank_prefix = "ST";
    bool collector_timing_bank_enabled = true;
    std::string collector_timing_bank_prefix = "SC";
    bool advanced_bank_enabled = true;
    std::string advanced_bank_prefix = "SH";
};

#endif
