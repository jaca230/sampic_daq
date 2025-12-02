#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace sampic::deadtime {

struct ChannelSet {
  std::string label;
  std::vector<int> channels;
};

struct ParameterCombination {
  int pulser_period_us = 0;
  int digitizer_rate_mhz = 0;
  ChannelSet channel_set;
};

struct ConnectionConfig {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib";
  double threshold_volts = 0.1;
  bool pulser_sync = false;
  bool use_external_clock = false;
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
  std::vector<int> pulser_period_us;
  std::vector<int> digitizer_rates_mhz;
  std::vector<ChannelSet> channel_sets;
  std::vector<int> channel_counts;
  int board_index = 0;
};

struct OutputConfig {
  std::filesystem::path results_path;
};

struct DeadtimeConfig {
  ConnectionConfig connection;
  ReadoutConfig readout;
  TimingConfig timing;
  StartRetryConfig start_retry;
  ScanConfig scan;
  OutputConfig output;
};

DeadtimeConfig load_deadtime_config(const std::string& path, nlohmann::json::parser_callback_t cb = nullptr);

std::vector<ParameterCombination> build_parameter_space(const ScanConfig& scan);

std::string make_combo_key(const ParameterCombination& combo, int board_index);

std::string make_combo_key_from_json(const nlohmann::json& record);

}  // namespace sampic::deadtime

