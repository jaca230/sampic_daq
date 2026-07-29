#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>

/// Collector mode selector
enum class SampicCollectorModeType {
    DEFAULT,
    EXAMPLE,
    SIMULATOR,
    SIMULATOR_PP_TRIG
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
    std::uint32_t hits_per_event = 1;

    /// Waveform samples (clamped to hardware max)
    std::uint32_t waveform_length = 64;

    /// Optional sleep to simulate hardware read latency (microseconds)
    std::uint32_t simulate_read_time_us = 0;

    /// Nominal spacing between hits inside an event (nanoseconds)
    double hit_time_step_ns = 5.0;

    /// Minimum time separation between successive events (nanoseconds)
    double inter_event_gap_ns = 1'500'000.0; // 1.5 ms default

    /// Initial timestamp assigned to the first synthetic event (nanoseconds)
    double start_timestamp_ns = 0.0;

    /// Baseline level (fraction of ADC full-scale) used for the template waveform
    double baseline_level = 0.05;

    /// Peak amplitude (fraction of ADC full-scale) used for the template waveform
    double signal_amplitude = 0.8;

    /// Nominal time-over-threshold assigned to each hit (nanoseconds)
    double tot_value_ns = 120.0;
};

/// Trigger-driven simulator mode (software-generated events gated by parport triggers)
struct SampicCollectorModeSimulatorParportTriggerConfig {
    /// Number of synthetic events generated for each received trigger
    std::uint32_t events_per_trigger = 1;

    /// Number of hits to synthesize in each event
    std::uint32_t hits_per_event = 1;

    /// Waveform samples (clamped to hardware max)
    std::uint32_t waveform_length = 64;

    /// Nominal spacing between hits inside an event (nanoseconds)
    double hit_time_step_ns = 5.0;

    /// Minimum time separation between successive events (nanoseconds)
    double inter_event_gap_ns = 1'500'000.0; // 1.5 ms default

    /// Initial timestamp assigned to the first synthetic event (nanoseconds)
    double start_timestamp_ns = 0.0;

    /// Baseline level (fraction of ADC full-scale) used for the template waveform
    double baseline_level = 0.05;

    /// Peak amplitude (fraction of ADC full-scale) used for the template waveform
    double signal_amplitude = 0.8;

    /// Nominal time-over-threshold assigned to each hit (nanoseconds)
    double tot_value_ns = 120.0;

    /// UNIX socket where parport trigger server publishes events
    std::string socket_path = "/tmp/parport_trigger.sock";

    /// Trigger character device used by the server
    std::string device_path = "/dev/parport_trigger";

    /// Start an in-process server if no server is listening on socket_path
    bool auto_start_server = true;

    /// Timeout for client connect/reconnect attempts (milliseconds)
    std::uint32_t connect_timeout_ms = 2000;

    /// Retry delay for client reconnect loop (milliseconds)
    std::uint32_t retry_interval_ms = 100;

    /// Queue depth for incoming trigger notifications
    std::uint32_t queue_capacity = 4096;

    /// Poll timeout used by in-process parport server (milliseconds)
    std::uint32_t server_poll_timeout_ms = 25;

    /// Wait timeout in collect() for next trigger event (milliseconds)
    std::uint32_t wait_timeout_ms = 10;
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
    SampicCollectorModeSimulatorParportTriggerConfig simulator_pp_trig_mode;
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
