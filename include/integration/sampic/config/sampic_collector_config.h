#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <map>
#include <string>
#include <cstddef>

/// Collector mode selector
enum class SampicCollectorModeType {
    DEFAULT,
    EXAMPLE
};

/// Default collector mode configuration
struct SampicCollectorModeDefaultConfig : public SampicCollectorCommonParams {
    int soft_trigger_prepare_interval = 100;   // how often to re-call PrepareEvent
    int soft_trigger_max_loops        = 10000; // max loops before timing out
    int soft_trigger_retry_sleep_us   = 100;   // sleep between failed read retries
};

/// Example collector mode configuration (placeholder)
struct SampicCollectorModeExampleConfig : public SampicCollectorCommonParams {
    int soft_trigger_prepare_interval = 100;   // how often to re-call PrepareEvent
    int soft_trigger_max_loops        = 10000; // max loops before timing out
    int soft_trigger_retry_sleep_us   = 100;   // sleep between failed read retries
};

/// Top-level collector configuration
struct SampicCollectorConfig {
    // Mode selection
    SampicCollectorModeType mode = SampicCollectorModeType::DEFAULT;

    // Global parameters
    size_t buffer_size = 128;     // number of events the buffer can hold
    int sleep_time_us = 1000000;  // microseconds between collector polls

    // Per-mode configuration
    std::map<std::string, SampicCollectorModeDefaultConfig> default_mode {
        {"default_mode", {}}
    };

    std::map<std::string, SampicCollectorModeExampleConfig> example_mode {
        {"example_mode", {}}
    };
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
