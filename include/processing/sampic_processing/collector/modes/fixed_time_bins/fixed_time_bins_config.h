#ifndef FRONTEND_COLLECTOR_FIXED_TIME_BINS_CONFIG_H
#define FRONTEND_COLLECTOR_FIXED_TIME_BINS_CONFIG_H
#include <cstdint>
#include <string>

struct FrontendCollectorModeFixedTimeBinsConfig {
    double bin_width_ns = 10000.0;
    double bin_origin_ns = 0.0;
    double allowed_lateness_ns = 1000000.0;
    std::uint32_t idle_flush_ms = 10;
    std::uint32_t wait_timeout_ms = 10;
    std::uint32_t max_pending_bins = 10000;
    std::uint32_t max_bins_per_collect = 256;
    bool emit_empty_bins = true;
    bool data_bank_enabled = true;
    std::string data_bank_prefix = "SD";
    bool advanced_bank_enabled = true;
    std::string advanced_bank_prefix = "SH";
    bool verbose_banks = false;
    std::string bin_bank_prefix = "SB";
    bool event_timing_bank_enabled = true;
    std::string event_timing_bank_prefix = "ST";
};
#endif
