#include "sampic_tests/modes/deadtime/config.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <sstream>

namespace sampic::deadtime {

namespace {

template <typename T>
std::vector<T> read_int_array(const nlohmann::json& node, const std::string& key) {
  if (!node.contains(key)) {
    throw std::runtime_error("Config missing array: " + key);
  }
  if (!node.at(key).is_array()) {
    throw std::runtime_error("Config field '" + key + "' must be an array");
  }
  std::vector<T> values;
  for (const auto& item : node.at(key)) {
    if (!item.is_number_integer()) {
      throw std::runtime_error("Config field '" + key + "' must contain integers");
    }
    values.push_back(item.get<T>());
  }
  return values;
}

ChannelSet make_channel_set_from_count(int count) {
  if (count <= 0 || count > 64) {
    throw std::runtime_error("Channel count must be between 1 and 64");
  }
  ChannelSet set;
  set.label = std::to_string(count) + "ch_contiguous";
  set.channels.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    set.channels.push_back(i);
  }
  return set;
}

std::vector<ChannelSet> parse_channel_sets(const nlohmann::json& scan_node) {
  std::vector<ChannelSet> sets;
  if (!scan_node.contains("channel_sets")) return sets;
  const auto& arr = scan_node.at("channel_sets");
  if (!arr.is_array()) {
    throw std::runtime_error("scan.channel_sets must be an array");
  }
  for (const auto& entry : arr) {
    ChannelSet set;
    if (entry.is_array()) {
      set.channels = entry.get<std::vector<int>>();
    } else if (entry.is_object()) {
      if (!entry.contains("channels")) {
        throw std::runtime_error("channel_set object missing 'channels'");
      }
      set.channels = entry.at("channels").get<std::vector<int>>();
      if (entry.contains("label") && entry.at("label").is_string()) {
        set.label = entry.at("label").get<std::string>();
      }
    } else {
      throw std::runtime_error("channel_sets entries must be arrays or objects");
    }
    if (set.channels.empty()) {
      throw std::runtime_error("channel_set contains no channel indices");
    }
    std::set<int> dedup(set.channels.begin(), set.channels.end());
    set.channels.assign(dedup.begin(), dedup.end());
    if (!std::is_sorted(set.channels.begin(), set.channels.end())) {
      std::sort(set.channels.begin(), set.channels.end());
    }
    for (int ch : set.channels) {
      if (ch < 0 || ch >= 64) {
        throw std::runtime_error("channel index out of range (0-63): " + std::to_string(ch));
      }
    }
    if (set.label.empty()) {
      set.label = std::to_string(set.channels.size()) + "ch_custom";
    }
    sets.push_back(std::move(set));
  }
  return sets;
}

}  // namespace

DeadtimeConfig load_deadtime_config(const std::string& path, nlohmann::json::parser_callback_t cb) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Unable to open config file: " + path);
  }
  const nlohmann::json doc = nlohmann::json::parse(in, cb, true, true);

  DeadtimeConfig cfg;
  if (doc.contains("connection")) {
    const auto& node = doc.at("connection");
    if (node.contains("ip")) cfg.connection.ip = node.at("ip").get<std::string>();
    if (node.contains("port")) cfg.connection.port = node.at("port").get<int>();
    if (node.contains("load_calibration")) cfg.connection.load_calibration = node.at("load_calibration").get<bool>();
    if (node.contains("calibration_dir")) cfg.connection.calibration_dir = node.at("calibration_dir").get<std::string>();
    if (node.contains("threshold_volts")) cfg.connection.threshold_volts = node.at("threshold_volts").get<double>();
    if (node.contains("pulser_sync")) cfg.connection.pulser_sync = node.at("pulser_sync").get<bool>();
    if (node.contains("use_external_clock")) cfg.connection.use_external_clock = node.at("use_external_clock").get<bool>();
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
  cfg.scan.pulser_period_us = read_int_array<int>(scan_node, "pulser_period_us");
  cfg.scan.digitizer_rates_mhz = read_int_array<int>(scan_node, "digitizer_rates_mhz");
  cfg.scan.channel_sets = parse_channel_sets(scan_node);
  if (cfg.scan.channel_sets.empty()) {
    cfg.scan.channel_counts = read_int_array<int>(scan_node, "channel_counts");
    for (int count : cfg.scan.channel_counts) {
      cfg.scan.channel_sets.push_back(make_channel_set_from_count(count));
    }
  }

  if (!doc.contains("output") || !doc.at("output").contains("results_path")) {
    throw std::runtime_error("Config missing output.results_path");
  }
  cfg.output.results_path = doc.at("output").at("results_path").get<std::string>();

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
  if (cfg.scan.pulser_period_us.empty() || cfg.scan.digitizer_rates_mhz.empty() ||
      cfg.scan.channel_sets.empty()) {
    throw std::runtime_error("Parameter space arrays must not be empty");
  }
  return cfg;
}

std::vector<ParameterCombination> build_parameter_space(const ScanConfig& scan) {
  std::vector<ParameterCombination> combos;
  combos.reserve(scan.pulser_period_us.size() * scan.digitizer_rates_mhz.size() *
                 scan.channel_sets.size());
  for (int pulser_us : scan.pulser_period_us) {
    if (pulser_us <= 0) {
      throw std::runtime_error("Pulser period must be positive (µs)");
    }
    for (int rate : scan.digitizer_rates_mhz) {
      if (rate <= 0) {
        throw std::runtime_error("Digitizer rate must be positive (MHz)");
      }
      for (const auto& set : scan.channel_sets) {
        combos.push_back(ParameterCombination{
            pulser_us, rate, set});
      }
    }
  }
  return combos;
}

std::string make_combo_key(const ParameterCombination& combo, int board_index) {
  std::ostringstream oss;
  oss << "pulser_us=" << combo.pulser_period_us << ";rate=" << combo.digitizer_rate_mhz
      << ";board=" << board_index << ";channels=";
  for (std::size_t i = 0; i < combo.channel_set.channels.size(); ++i) {
    if (i) oss << ',';
    oss << combo.channel_set.channels[i];
  }
  return oss.str();
}

std::string make_combo_key_from_json(const nlohmann::json& record) {
  if (!record.contains("parameters")) return {};
  const auto& params = record.at("parameters");
  if (!params.contains("pulser_period_us") || !params.contains("digitizer_rate_mhz") ||
      !params.contains("channel_ids") || !params.contains("board_index")) {
    return {};
  }
  ParameterCombination combo;
  combo.pulser_period_us = params.at("pulser_period_us").get<int>();
  combo.digitizer_rate_mhz = params.at("digitizer_rate_mhz").get<int>();
  combo.channel_set.channels = params.at("channel_ids").get<std::vector<int>>();
  combo.channel_set.label = params.value("channel_label", "");
  const int board = params.at("board_index").get<int>();
  return make_combo_key(combo, board);
}

}  // namespace sampic::deadtime
