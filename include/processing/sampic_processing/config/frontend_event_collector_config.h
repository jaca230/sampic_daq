#ifndef FRONTEND_EVENT_COLLECTOR_CONFIG_H
#define FRONTEND_EVENT_COLLECTOR_CONFIG_H

#include <string>
#include <cstdint>
#include "processing/sampic_processing/config/frontend_collector_diagnostics_config.h"

/// Configuration for the frontend event collector that assembles
/// hardware-level events into higher-level FrontendEvents.
struct FrontendEventCollectorConfig {
    // --- Global settings ---
    std::string mode = "default";

    /// Buffer size for assembled events.
    uint32_t buffer_size = 512;

    /// Microseconds to sleep between collection cycles.
    uint32_t sleep_time_us = 1000;

    FrontendCollectorDiagnosticsConfig diagnostics;
};

#endif // FRONTEND_EVENT_COLLECTOR_CONFIG_H
