#ifndef FRONTEND_EVENT_COLLECTOR_CONFIG_H
#define FRONTEND_EVENT_COLLECTOR_CONFIG_H

#include <string>
#include <cstdint>

/// Available modes for the frontend event collector.
enum class FrontendCollectorModeType {
    DEFAULT,
    EXAMPLE
};

/// Configuration for the default frontend collector mode.
struct FrontendCollectorModeDefaultConfig {
    // ------------------------------------------------------------------
    // Timing parameters
    // ------------------------------------------------------------------

    /// Maximum time difference (ns) between hits to group into the same event.
    double time_window_ns = 1000000.0;

    /// Maximum time to wait before finalizing a partial event (ms).
    double finalize_after_ms = 10.0;

    /// Milliseconds to wait for new SAMPIC events before timing out
    /// (used in the blocking wait inside the mode).
    uint32_t wait_timeout_ms = 1000;

    // ------------------------------------------------------------------
    // Bank prefixes
    // ------------------------------------------------------------------

    /// Prefix for waveform/scalar data banks (e.g., "AD00" for FE index 0).
    std::string data_bank_prefix = "AD";

    /// Prefix for event-level timing banks (per FrontendEvent, e.g. "AT00").
    std::string event_timing_bank_prefix = "AT";

    /// Prefix for collector-level timing banks (once per collection loop, e.g. "AC00").
    std::string collector_timing_bank_prefix = "AC";
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

    /// Buffer size for assembled events.
    uint32_t buffer_size = 512;

    /// Microseconds to sleep between collection cycles.
    uint32_t sleep_time_us = 1000;

    // --- Mode configurations ---
    FrontendCollectorModeDefaultConfig default_mode;
    FrontendCollectorModeExampleConfig example_mode;
};

#endif // FRONTEND_EVENT_COLLECTOR_CONFIG_H
