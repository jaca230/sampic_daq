#ifndef SAMPIC_COLLECTOR_MODE_SIMULATOR_CONFIG_H
#define SAMPIC_COLLECTOR_MODE_SIMULATOR_CONFIG_H

#include <cstdint>

struct SampicCollectorModeSimulatorConfig {
    std::uint32_t events_per_cycle = 1;
    std::uint32_t hits_per_event = 1;
    std::uint32_t waveform_length = 64;
    std::uint32_t simulate_read_time_us = 0;
    double hit_time_step_ns = 5.0;
    double inter_event_gap_ns = 1'500'000.0;
    double start_timestamp_ns = 0.0;
    double baseline_level = 0.05;
    double signal_amplitude = 0.8;
    double tot_value_ns = 120.0;
    double noise_rms_adc = 3.0;
    double amplitude_jitter_fraction = 0.05;
    double trigger_jitter_samples = 1.25;
    double punch_through_fraction = 0.75;
    double punch_amplitude_fraction = 0.20;
    std::uint32_t random_seed = 1;
};

#endif
