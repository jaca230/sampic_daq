#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <string>
#include <cstddef>

/// Collector mode selector
enum class SampicCollectorModeType {
    DEFAULT,
    EXAMPLE
};

/// Default collector mode configuration
struct SampicCollectorModeDefaultConfig {
    /// How often to re-call PrepareEvent
    int soft_trigger_prepare_interval = 100;

    /// Max loops before timing out
    int soft_trigger_max_loops = 10000;

    /// Sleep between failed read retries (µs)
    int soft_trigger_retry_sleep_us = 100;
};

/// Example collector mode configuration (placeholder)
struct SampicCollectorModeExampleConfig {
    int soft_trigger_prepare_interval = 100;
    int soft_trigger_max_loops = 10000;
    int soft_trigger_retry_sleep_us = 100;
};

/// Top-level collector configuration
struct SampicCollectorConfig {
    // --- Mode selection ---
    SampicCollectorModeType mode = SampicCollectorModeType::DEFAULT;

    // --- Global parameters ---
    /// Number of events the buffer can hold
    size_t buffer_size = 128;

    /// Microseconds between collector polls
    int sleep_time_us = 1'000'000;

    // --- Per-mode configurations ---
    SampicCollectorModeDefaultConfig default_mode;
    SampicCollectorModeExampleConfig example_mode;
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
