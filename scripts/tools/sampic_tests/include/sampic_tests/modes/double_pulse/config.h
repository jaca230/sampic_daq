#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

namespace sampic::double_pulse {

struct ChannelSet {
  std::string label;
  std::vector<int> channels;
};

struct ParameterCombination {
  double lecroy_frequency_hz = 0.0;
  int digitizer_rate_mhz = 0;
  double pulse_separation_ns = 0.0;
  bool auto_conversion = true;
  double lecroy_amplitude_v = 0.0;
  double threshold_volts = 0.1;
  std::string channel_label;
  std::vector<int> channels;
};

struct ConnectionConfig {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib";
  double threshold_volts = 0.1;
  bool use_external_clock = false;
  bool use_external_trigger = true;
};

struct BinarySearchConfig {
  double min_ns = 10.0;
  double max_ns = 1.0e6;
  double start_ns = 1000.0;
  double tolerance_ns = 50.0;
  int max_iterations = 20;
  double ratio_threshold = 1.5;
};

struct ReadoutConfig {
  int prepare_interval = 100;
  int max_loops = 10000;
  int retry_sleep_us = 100;
};

struct TimingConfig {
  double sample_duration_s = 3.0;
  int samples_per_combo = 5;
  double cooldown_between_combos_s = 0.0;
};

struct StartRetryConfig {
  int max_attempts = 5;
  double initial_delay_s = 10.0;
  double backoff = 2.0;
};

struct ScanConfig {
  std::vector<int> digitizer_rates_mhz;
  std::vector<double> lecroy_rates_hz;
  std::vector<double> legacy_pulse_separation_ns;
  std::vector<int> channels;
  std::vector<ChannelSet> channel_sets;
  std::vector<bool> auto_conversion_modes;
  std::vector<double> lecroy_amplitudes_v;
  std::vector<double> thresholds_volts;
  int board_index = 0;
};

struct OutputConfig {
  std::filesystem::path results_path;
};

struct ExternalTriggerConfig {
  ExternalTriggerType_t trigger_type = EXT_SIG;
  EdgeType_t trigger_edge = RISING_EDGE;
  SignalLevel_t trigger_level = TTL_SIG;
  EdgeType_t sync_edge = RISING_EDGE;
  SignalLevel_t sync_level = TTL_SIG;
};

struct DoublePulseConfig {
  ConnectionConfig connection;
  ExternalTriggerConfig external_trigger;
  ReadoutConfig readout;
  TimingConfig timing;
  StartRetryConfig start_retry;
  ScanConfig scan;
  sampic::lecroy::LecroyConfig lecroy;
  BinarySearchConfig search;
  OutputConfig output;
};

DoublePulseConfig load_double_pulse_config(const std::string& path);

std::vector<ParameterCombination> build_parameter_space(const ScanConfig& scan);

std::string make_combo_key(const ParameterCombination& combo, int board_index);

std::string make_combo_key_from_json(const nlohmann::json& record);

}  // namespace sampic::double_pulse
