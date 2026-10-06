#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_DIAGNOSTICS_CONFIG_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_DIAGNOSTICS_CONFIG_H

#include <cstdint>

struct FrontendCollectorDiagnosticsConfig {
    bool enabled = false;
    std::uint32_t log_interval_ms = 1000;
    std::uint32_t buffer_warning_threshold = 256;
    bool log_hit_details = false;
    bool log_group_details = false;
};

#endif
