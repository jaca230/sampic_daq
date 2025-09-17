#ifndef SAMPIC_CONFIG_H
#define SAMPIC_CONFIG_H

#include <string>
#include <cstdint>
#include <rfl.hpp>

// Basic acquisition settings
struct SampicAcquisitionSettings {
    int sampling_frequency = 3200;  // MHz, default 3.2 GHz
    bool use_external_clock = false;
    bool smart_read_mode = false;
    int samples_to_read = 1024;  // MAX_NB_OF_SAMPLES from library
    int read_offset = 0;
    int adc_bits = 11;  // 8, 9, 10, or 11 bits
    bool auto_conversion = true;
    int frames_per_block = 1;
    bool tot_measurement = false;
};

// Trigger configuration
struct SampicTriggerSettings {
    std::string external_trigger_type = "SOFTWARE";  // SOFTWARE, HARDWARE, etc.
    std::string trigger_edge = "RISING";  // RISING, FALLING
    std::string signal_level = "NIM";  // NIM, TTL
    bool enable_trigger_counter = false;
    bool detect_external_trigger_id = false;
    std::uint8_t triggers_per_event = 1;
    bool level2_trigger_build = false;
    bool level3_trigger_build = false;
};

// Pulser settings
struct SampicPulserSettings {
    bool enable = false;
    std::string source = "INTERNAL";  // INTERNAL, EXTERNAL
    bool synchronous = true;
    int period = 1000;  // Clock cycles
    std::uint8_t width = 10;  // Multiple of 10ns
};

// System-wide synchronization settings
struct SampicSyncSettings {
    bool sync_mode = false;
    bool master_mode = false;
    bool coincidence_mode = false;
};

// Calibration settings
struct SampicCalibrationSettings {
    bool adc_linearity_correction = false;
    bool time_inl_correction = false;
    bool residual_pedestal_correction = false;
    std::string calibration_directory = ".";
    
    // Auto INL calibration
    bool auto_inl_calib = false;
    bool digital_level = false;
    int inl_frequency = 3;  // 0-7
    int rising_edge_slope = 7;  // 0-15
    int falling_edge_slope = 7;  // 0-15
    int high_level = 7;  // 0-7
    int low_level = 0;  // 0-7
};

// Single channel configuration
struct SampicChannelConfig {
    bool enabled = true;
    std::string trigger_mode = "EXTERNAL";  // EXTERNAL, INTERNAL, DISABLED
    float internal_threshold = 0.1f;  // Volts relative to baseline
    std::string trigger_edge = "FALLING";  // RISING, FALLING
    bool external_threshold_mode = false;
    bool enable_for_central_trigger = true;
    bool pulse_mode = false;
};

// All channels for one SAMPIC (16 channels)
struct SampicChannelSettings {
    SampicChannelConfig channel0;
    SampicChannelConfig channel1;
    SampicChannelConfig channel2;
    SampicChannelConfig channel3;
    SampicChannelConfig channel4;
    SampicChannelConfig channel5;
    SampicChannelConfig channel6;
    SampicChannelConfig channel7;
    SampicChannelConfig channel8;
    SampicChannelConfig channel9;
    SampicChannelConfig channel10;
    SampicChannelConfig channel11;
    SampicChannelConfig channel12;
    SampicChannelConfig channel13;
    SampicChannelConfig channel14;
    SampicChannelConfig channel15;
};

// Single SAMPIC chip configuration
struct SampicChipConfig {
    float baseline_reference = 0.8f;  // Volts, 0-1.6V range
    float external_threshold = 0.5f;  // Volts
    std::string central_trigger_mode = "OR";  // OR, MULTIPLICITY_2, MULTIPLICITY_3
    std::string central_trigger_effect = "TRIGGER";
    bool enable_post_trigger = false;
    int post_trigger_value = 0;  // 0-7
    
    // TOT (Time Over Threshold) settings
    std::string tot_range = "MAX_25_NS";  // MAX_25_NS, MAX_50_NS, MAX_100_NS, MAX_200_NS, MAX_400_NS
    bool tot_filter_enable = false;
    bool tot_filter_wide_cap = false;
    float tot_filter_pulse_width = 10.0f;  // ns
    
    // Advanced DAC settings
    float vdac_dll = 1.0f;  // DLL voltage
    float vdac_ramp = 1.2f;  // ADC ramp voltage
    float vdac_dll_continuity = 1.0f;
    std::string dll_mode = "MEDIUM";  // ULTRA_SLOW, SLOW, MEDIUM, FAST
    
    // Channels for this SAMPIC
    SampicChannelSettings channels;
};

// All SAMPICs for one front-end board (4 SAMPICs)
struct SampicChipSettings {
    SampicChipConfig sampic0;
    SampicChipConfig sampic1;
    SampicChipConfig sampic2;
    SampicChipConfig sampic3;
};

// Single front-end board configuration
struct SampicFrontEndConfig {
    bool trigger_enable = true;
    std::string global_trigger_option = "L2";  // L2, L3
    bool coincidence_with_ext_trigger = false;
    std::uint8_t external_trigger_gate = 10;
    bool common_dead_time = false;
    
    // SAMPICs for this front-end board
    SampicChipSettings sampics;
};

// All front-end boards (4 boards)
struct SampicFrontEndSettings {
    SampicFrontEndConfig feb0;
    SampicFrontEndConfig feb1;
    SampicFrontEndConfig feb2;
    SampicFrontEndConfig feb3;
};

// Main SAMPIC system configuration
struct SampicSystemSettings {
    // Connection settings
    std::string ip_address = "192.168.0.4";
    int port = 27015;
    std::string connection_type = "UDP";  // UDP, USB
    
    // Basic settings
    SampicAcquisitionSettings acquisition;
    SampicTriggerSettings trigger;
    SampicPulserSettings pulser;
    SampicSyncSettings synchronization;
    SampicCalibrationSettings calibration;
    
    // Hardware settings
    SampicFrontEndSettings front_end_boards;
    
    // System info
    bool verbose = false;
    int polling_interval_us = 1000000;
    int events_per_read = 1;
};

#endif // SAMPIC_CONFIG_H