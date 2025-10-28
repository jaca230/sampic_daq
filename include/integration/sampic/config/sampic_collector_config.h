#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <string>
#include <cstddef>
#include <cstdint>

/// Collector mode selector
enum class SampicCollectorModeType {
    DEFAULT,
    EXAMPLE,
    SIMULATOR
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

/// Simulator collector mode configuration (software-generated events)
struct SampicCollectorModeSimulatorConfig {
    /// Number of synthetic events generated per collect() invocation
    std::uint32_t events_per_cycle = 1;

    /// Number of hits to synthesize in each event
    std::uint32_t hits_per_event = 16;

    /// Waveform samples (clamped to hardware max)
    std::uint32_t waveform_length = 64;

    /// Optional sleep to simulate hardware read latency (microseconds)
    std::uint32_t simulate_read_time_us = 0;
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
    SampicCollectorModeSimulatorConfig simulator_mode;
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
