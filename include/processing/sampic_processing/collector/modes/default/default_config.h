#ifndef FRONTEND_COLLECTOR_MODE_DEFAULT_CONFIG_H
#define FRONTEND_COLLECTOR_MODE_DEFAULT_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeDefaultConfig {
    double time_window_ns = 1000000.0;
    double finalize_after_ms = 10.0;
    std::uint32_t wait_timeout_ms = 1000;
    std::string data_bank_prefix = "AD";
    std::string event_timing_bank_prefix = "AT";
    std::string collector_timing_bank_prefix = "AC";
};

#endif
