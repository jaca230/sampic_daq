#ifndef FRONTEND_COLLECTOR_MODE_EXTERNAL_TRIGGER_CONFIG_H
#define FRONTEND_COLLECTOR_MODE_EXTERNAL_TRIGGER_CONFIG_H

#include <cstdint>
#include <string>

struct FrontendCollectorModeExternalTriggerConfig {
    double hit_time_offset_ns = -470.0;
    double pre_window_ns = 20.0;
    double post_window_ns = 20.0;
    double sampling_frequency_mhz = 6400.0;
    std::uint32_t wait_timeout_ms = 1000;
    bool emit_triggers_without_hits = true;
    std::string data_bank_prefix = "AD";
    std::string event_timing_bank_prefix = "AT";
    std::string trigger_metadata_bank_prefix = "TG";
};

#endif
