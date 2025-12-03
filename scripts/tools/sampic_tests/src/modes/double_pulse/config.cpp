#include "sampic_tests/modes/double_pulse/config.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace sampic::double_pulse {

namespace {

template <typename T>
std::vector<T> read_array(const nlohmann::json& node,
                          const std::string& key,
                          bool integers_only) {
  if (!node.contains(key)) {
    throw std::runtime_error("Config missing array: " + key);
  }
  if (!node.at(key).is_array()) {
    throw std::runtime_error("Config field '" + key + "' must be an array");
  }
  std::vector<T> values;
  for (const auto& item : node.at(key)) {
    if (integers_only && !item.is_number_integer()) {
      throw std::runtime_error("Config field '" + key + "' must contain integers");
    }
    values.push_back(item.get<T>());
  }
  return values;
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return value;
}

ExternalTriggerType_t parse_trigger_type(const std::string& text) {
  const auto value = uppercase(text);
  if (value == "SOFTWARE") return SOFTWARE;
  if (value == "INTERNAL" || value == "INTERNAL_OSC") return INTERNAL_OSC;
  if (value == "EXT_SIG" || value == "EXTERNAL") return EXT_SIG;
  throw std::runtime_error("Unknown external trigger type: " + text);
}

EdgeType_t parse_edge(const std::string& text) {
  const auto value = uppercase(text);
  if (value == "RISING" || value == "RISING_EDGE") return RISING_EDGE;
  if (value == "FALLING" || value == "FALLING_EDGE") return FALLING_EDGE;
  throw std::runtime_error("Unknown edge type: " + text);
}

SignalLevel_t parse_signal_level(const std::string& text) {
  const auto value = uppercase(text);
  if (value == "TTL" || value == "TTL_SIG") return TTL_SIG;
  if (value == "NIM" || value == "NIM_SIG") return NIM_SIG;
  throw std::runtime_error("Unknown signal level: " + text);
}

}  // namespace

DoublePulseConfig load_double_pulse_config(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Unable to open config file: " + path);
  }
  const nlohmann::json doc = nlohmann::json::parse(in, nullptr, true, true);

  DoublePulseConfig cfg;
  if (doc.contains("connection")) {
    const auto& node = doc.at("connection");
    if (node.contains("ip")) cfg.connection.ip = node.at("ip").get<std::string>();
    if (node.contains("port")) cfg.connection.port = node.at("port").get<int>();
    if (node.contains("load_calibration")) cfg.connection.load_calibration = node.at("load_calibration").get<bool>();
    if (node.contains("calibration_dir")) cfg.connection.calibration_dir = node.at("calibration_dir").get<std::string>();
    if (node.contains("threshold_volts")) cfg.connection.threshold_volts = node.at("threshold_volts").get<double>();
    if (node.contains("use_external_clock")) cfg.connection.use_external_clock = node.at("use_external_clock").get<bool>();
    if (node.contains("use_external_trigger")) cfg.connection.use_external_trigger = node.at("use_external_trigger").get<bool>();
  }

  if (doc.contains("binary_search")) {
    const auto& node = doc.at("binary_search");
    if (node.contains("min_ns")) cfg.search.min_ns = node.at("min_ns").get<double>();
    if (node.contains("max_ns")) cfg.search.max_ns = node.at("max_ns").get<double>();
    if (node.contains("start_ns")) cfg.search.start_ns = node.at("start_ns").get<double>();
    if (node.contains("tolerance_ns")) cfg.search.tolerance_ns = node.at("tolerance_ns").get<double>();
    if (node.contains("max_iterations")) cfg.search.max_iterations = node.at("max_iterations").get<int>();
    if (node.contains("ratio_threshold")) cfg.search.ratio_threshold = node.at("ratio_threshold").get<double>();
  }

  if (doc.contains("external_trigger")) {
    const auto& node = doc.at("external_trigger");
    if (node.contains("type")) cfg.external_trigger.trigger_type =
        parse_trigger_type(node.at("type").get<std::string>());
    if (node.contains("edge")) cfg.external_trigger.trigger_edge =
        parse_edge(node.at("edge").get<std::string>());
    if (node.contains("signal_level")) cfg.external_trigger.trigger_level =
        parse_signal_level(node.at("signal_level").get<std::string>());
    if (node.contains("sync_edge")) cfg.external_trigger.sync_edge =
        parse_edge(node.at("sync_edge").get<std::string>());
    if (node.contains("sync_signal_level")) cfg.external_trigger.sync_level =
        parse_signal_level(node.at("sync_signal_level").get<std::string>());
  }

  if (doc.contains("readout")) {
    const auto& node = doc.at("readout");
    if (node.contains("prepare_interval")) cfg.readout.prepare_interval = node.at("prepare_interval").get<int>();
    if (node.contains("max_loops")) cfg.readout.max_loops = node.at("max_loops").get<int>();
    if (node.contains("retry_sleep_us")) cfg.readout.retry_sleep_us = node.at("retry_sleep_us").get<int>();
  }

  if (doc.contains("timing")) {
    const auto& node = doc.at("timing");
    if (node.contains("sample_duration_s")) cfg.timing.sample_duration_s = node.at("sample_duration_s").get<double>();
    if (node.contains("samples_per_combo")) cfg.timing.samples_per_combo = node.at("samples_per_combo").get<int>();
    if (node.contains("cooldown_between_combos_s")) cfg.timing.cooldown_between_combos_s = node.at("cooldown_between_combos_s").get<double>();
  }

  if (doc.contains("start_retry")) {
    const auto& node = doc.at("start_retry");
    if (node.contains("max_attempts")) cfg.start_retry.max_attempts = node.at("max_attempts").get<int>();
    if (node.contains("initial_delay_s")) cfg.start_retry.initial_delay_s = node.at("initial_delay_s").get<double>();
    if (node.contains("backoff")) cfg.start_retry.backoff = node.at("backoff").get<double>();
  }

  if (!doc.contains("scan")) {
    throw std::runtime_error("Config missing 'scan' section");
  }
  const auto& scan_node = doc.at("scan");
  cfg.scan.board_index = scan_node.value("board_index", 0);
  cfg.scan.digitizer_rates_mhz = read_array<int>(scan_node, "digitizer_rates_mhz", true);
  if (scan_node.contains("lecroy_rates_hz")) {
    cfg.scan.lecroy_rates_hz = read_array<double>(scan_node, "lecroy_rates_hz", false);
  } else {
    cfg.scan.lecroy_rates_hz = {cfg.lecroy.frequency_hz};
  }
  if (scan_node.contains("double_pulse_delays_ns")) {
    cfg.scan.legacy_pulse_separation_ns =
        read_array<double>(scan_node, "double_pulse_delays_ns", false);
  }
  cfg.scan.channels = read_array<int>(scan_node, "channels", true);
  if (cfg.scan.channels.empty()) {
    throw std::runtime_error("scan.channels must list at least one channel index");
  }
  std::set<int> sorted(cfg.scan.channels.begin(), cfg.scan.channels.end());
  cfg.scan.channels.assign(sorted.begin(), sorted.end());
  for (int ch : cfg.scan.channels) {
    if (ch < 0 || ch >= 64) {
      throw std::runtime_error("Channel index out of range (0-63): " + std::to_string(ch));
    }
  }

  if (!doc.contains("output") || !doc.at("output").contains("results_path")) {
    throw std::runtime_error("Config missing output.results_path");
  }
  cfg.output.results_path = doc.at("output").at("results_path").get<std::string>();

  if (doc.contains("lecroy")) {
    const auto& node = doc.at("lecroy");
    if (node.contains("ip")) cfg.lecroy.ip = node.at("ip").get<std::string>();
    if (node.contains("port")) cfg.lecroy.port = node.at("port").get<int>();
    if (node.contains("frequency_hz")) cfg.lecroy.frequency_hz = node.at("frequency_hz").get<double>();
    if (node.contains("trigger_mode")) cfg.lecroy.trigger_mode = node.at("trigger_mode").get<std::string>();
    if (node.contains("trigger_slope")) cfg.lecroy.trigger_slope = node.at("trigger_slope").get<std::string>();
    if (node.contains("trigger_level_volts")) cfg.lecroy.trigger_level_volts = node.at("trigger_level_volts").get<double>();
    if (node.contains("initial_delay_ns")) cfg.lecroy.initial_delay_ns = node.at("initial_delay_ns").get<double>();
    if (node.contains("manual_trigger")) cfg.lecroy.manual_trigger = node.at("manual_trigger").get<bool>();
    if (node.contains("manual_trigger_interval_s")) {
      cfg.lecroy.manual_trigger_interval_s = node.at("manual_trigger_interval_s").get<double>();
    }
    if (node.contains("settle_delay_s")) cfg.lecroy.settle_delay_s = node.at("settle_delay_s").get<double>();
    if (node.contains("channel")) cfg.lecroy.channel.channel = node.at("channel").get<std::string>();
    if (node.contains("amplitude_v")) cfg.lecroy.channel.amplitude_v = node.at("amplitude_v").get<double>();
    if (node.contains("baseline_v")) cfg.lecroy.channel.baseline_v = node.at("baseline_v").get<double>();
    if (node.contains("width_ns")) cfg.lecroy.channel.width_ns = node.at("width_ns").get<double>();
    if (node.contains("lead_ns")) cfg.lecroy.channel.lead_ns = node.at("lead_ns").get<double>();
    if (node.contains("trail_ns")) cfg.lecroy.channel.trail_ns = node.at("trail_ns").get<double>();
    if (node.contains("output_main")) cfg.lecroy.channel.output_main = node.at("output_main").get<bool>();
    if (node.contains("output_inverse")) cfg.lecroy.channel.output_inverse = node.at("output_inverse").get<bool>();
    if (node.contains("double_pulse_enabled")) cfg.lecroy.channel.double_pulse_enabled = node.at("double_pulse_enabled").get<bool>();
  }

  if (cfg.timing.sample_duration_s <= 0.0) {
    throw std::runtime_error("sample_duration_s must be > 0");
  }
  if (cfg.timing.samples_per_combo <= 0) {
    throw std::runtime_error("samples_per_combo must be > 0");
  }
  if (cfg.start_retry.max_attempts <= 0) {
    throw std::runtime_error("start_retry.max_attempts must be > 0");
  }
  if (cfg.start_retry.initial_delay_s <= 0.0) {
    throw std::runtime_error("start_retry.initial_delay_s must be > 0");
  }
  if (cfg.start_retry.backoff < 1.0) {
    throw std::runtime_error("start_retry.backoff must be >= 1.0");
  }
  if (cfg.scan.lecroy_rates_hz.empty() || cfg.scan.digitizer_rates_mhz.empty()) {
    throw std::runtime_error("Scan parameter arrays must not be empty");
  }
  if (cfg.search.min_ns <= 0.0 || cfg.search.max_ns <= 0.0 ||
      cfg.search.min_ns >= cfg.search.max_ns) {
    throw std::runtime_error("binary_search bounds must be positive and min < max");
  }
  if (cfg.search.start_ns < cfg.search.min_ns || cfg.search.start_ns > cfg.search.max_ns) {
    throw std::runtime_error("binary_search.start_ns must lie between min and max");
  }
  if (cfg.search.tolerance_ns <= 0.0) {
    throw std::runtime_error("binary_search.tolerance_ns must be > 0");
  }
  if (cfg.search.max_iterations <= 0) {
    throw std::runtime_error("binary_search.max_iterations must be > 0");
  }
  return cfg;
}

std::vector<ParameterCombination> build_parameter_space(const ScanConfig& scan) {
  std::vector<ParameterCombination> combos;
  combos.reserve(scan.lecroy_rates_hz.size() * scan.digitizer_rates_mhz.size());
  for (double freq : scan.lecroy_rates_hz) {
    if (freq <= 0.0) {
      throw std::runtime_error("Lecroy frequency must be positive (Hz)");
    }
    for (int rate : scan.digitizer_rates_mhz) {
      if (rate <= 0) {
        throw std::runtime_error("Digitizer rate must be positive (MHz)");
      }
      ParameterCombination combo;
      combo.lecroy_frequency_hz = freq;
      combo.digitizer_rate_mhz = rate;
      combos.push_back(combo);
    }
  }
  return combos;
}

std::string make_combo_key(const ParameterCombination& combo, int board_index) {
  std::ostringstream oss;
  oss << "freq_hz=" << combo.lecroy_frequency_hz << ";rate=" << combo.digitizer_rate_mhz
      << ";board=" << board_index;
  return oss.str();
}

std::string make_combo_key_from_json(const nlohmann::json& record) {
  if (!record.contains("parameters")) return {};
  const auto& params = record.at("parameters");
  if (!params.contains("lecroy_frequency_hz") || !params.contains("digitizer_rate_mhz") ||
      !params.contains("board_index")) {
    return {};
  }
  ParameterCombination combo;
  combo.lecroy_frequency_hz = params.at("lecroy_frequency_hz").get<double>();
  combo.digitizer_rate_mhz = params.at("digitizer_rate_mhz").get<int>();
  const int board = params.at("board_index").get<int>();
  return make_combo_key(combo, board);
}

}  // namespace sampic::double_pulse
