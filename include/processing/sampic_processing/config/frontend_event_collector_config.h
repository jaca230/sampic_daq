#ifndef FRONTEND_EVENT_COLLECTOR_CONFIG_H
#define FRONTEND_EVENT_COLLECTOR_CONFIG_H

#include <string>
#include <cstdint>
#include <map>

/// Available modes for the frontend event collector.
enum class FrontendCollectorModeType {
    DEFAULT,
    EXAMPLE
};

/// Configuration for the default frontend collector mode.
struct FrontendCollectorModeDefaultConfig {
    // Maximum time difference (ns) between hits to group into the same event.
    double time_window_ns = 50.0;

    // Maximum time to wait before finalizing a partial event (ms).
    double finalize_after_ms = 10.0;
};

/// Example / placeholder mode configuration.
struct FrontendCollectorModeExampleConfig {
    int dummy_param = 0;
};

/// Configuration for the frontend event collector that assembles
/// hardware-level events into higher-level FrontendEvents.
struct FrontendEventCollectorConfig {
    // --- Global settings ---
    FrontendCollectorModeType mode = FrontendCollectorModeType::DEFAULT;

    // Buffer size for assembled events
    uint32_t buffer_size = 512;

    // Microseconds to sleep between polling cycles
    uint32_t sleep_time_us = 1000;

    // Optional upper bound for per-hit samples (safety clamp)
    uint32_t clamp_max_samples = 4096;

    // --- Per-mode configurations ---
    std::map<std::string, FrontendCollectorModeDefaultConfig> default_mode {
        {"default_mode", {}}
    };

    std::map<std::string, FrontendCollectorModeExampleConfig> example_mode {
        {"example_mode", {}}
    };
};

#endif // FRONTEND_EVENT_COLLECTOR_CONFIG_H
