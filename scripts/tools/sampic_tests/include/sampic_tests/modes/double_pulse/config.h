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
  double pulse_separation_ns = 0.0;
  int digitizer_rate_mhz = 0;
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
  std::vector<double> pulse_separation_ns;
  std::vector<int> channels;
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
  OutputConfig output;
};

DoublePulseConfig load_double_pulse_config(const std::string& path);

std::vector<ParameterCombination> build_parameter_space(const ScanConfig& scan);

std::string make_combo_key(const ParameterCombination& combo, int board_index);

std::string make_combo_key_from_json(const nlohmann::json& record);

}  // namespace sampic::double_pulse
