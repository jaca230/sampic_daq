#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <cstddef>
#include <string>

/// Modes for the collector loop
enum class SampicCollectorModeType {
    DEFAULT,
    EXAMPLE
};

/// Configuration for SampicCollector
struct SampicCollectorConfig {
    SampicCollectorModeType mode = SampicCollectorModeType::EXAMPLE;

    // Buffering
    size_t buffer_size = 128;      // number of events the buffer can hold

    // Timing
    int sleep_time_us = 1000000;    // microseconds to sleep between collector loop polls

    // Acquisition loop
    int soft_trigger_prepare_interval = 100;     // how often to re-call PrepareEvent
    int soft_trigger_max_loops        = 10000;   // max loops before timing out
    int soft_trigger_retry_sleep_us   = 100;     // NEW: sleep between failed read retries
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
