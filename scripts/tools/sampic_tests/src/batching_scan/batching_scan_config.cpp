#include "sampic_tests/batching_scan/batching_scan_config.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace sampic::batching_scan {
namespace {

template <typename T>
std::vector<T> required_array(
    const nlohmann::json& parent,
    const char* name) {
  if (!parent.contains(name) || !parent.at(name).is_array() ||
      parent.at(name).empty()) {
    throw std::runtime_error(
        std::string("Batching-scan config requires non-empty array '") +
        name + "'");
  }
  return parent.at(name).get<std::vector<T>>();
}

std::filesystem::path resolve_project_path(
    std::filesystem::path path,
    const std::filesystem::path& project_dir) {
  if (path.is_relative()) path = project_dir / path;
  return std::filesystem::weakly_canonical(path);
}

void validate(const BatchingScanConfig& config) {
  for (const int value : config.frames_per_block) {
    if (value < 1 || value > 31) {
      throw std::runtime_error("frames_per_block values must be in [1, 31]");
    }
  }
  for (const int value : config.triggers_per_event) {
    if (value < 1 || value > 127) {
      throw std::runtime_error("triggers_per_event values must be in [1, 127]");
    }
  }
  for (const double value : config.lecroy_rates_hz) {
    if (!std::isfinite(value) || value <= 0.0) {
      throw std::runtime_error("lecroy_rates_hz values must be positive");
    }
  }
  std::set<int> unique_channels;
  for (const int channel : config.enabled_channels) {
    if (channel < 0 || channel >= 64) {
      throw std::runtime_error("enabled_channels values must be in [0, 63]");
    }
    if (!unique_channels.insert(channel).second) {
      throw std::runtime_error("enabled_channels must not contain duplicates");
    }
  }
  if (config.repetitions < 1 || config.max_events < 1 ||
      config.duration_s <= 0.0 ||
      !std::isfinite(config.post_start_settle_s) ||
      config.post_start_settle_s < 0.0) {
    throw std::runtime_error(
        "repetitions, max_events, and duration_s must be positive; "
        "post_start_settle_s must be finite and non-negative");
  }
  if (config.raw_queue_capacity < 2 || config.raw_queue_capacity > 4096) {
    throw std::runtime_error("raw_queue_capacity must be in [2, 4096]");
  }
  if (config.startup_waveform_hits > 1000000) {
    throw std::runtime_error(
        "startup_waveform_hits must not exceed 1000000");
  }
  if (config.max_point_retries < 0 || config.retry_delay_s < 0.0) {
    throw std::runtime_error("retry settings must be non-negative");
  }
  if (config.lecroy_pulse_defaults.apply) {
    const auto& defaults = config.lecroy_pulse_defaults;
    if (!std::isfinite(defaults.amplitude_v) || defaults.amplitude_v <= 0.0 ||
        !std::isfinite(defaults.baseline_v) ||
        !std::isfinite(defaults.width_ns) || defaults.width_ns <= 0.0 ||
        !std::isfinite(defaults.lead_ns) || defaults.lead_ns <= 0.0 ||
        !std::isfinite(defaults.trail_ns) || defaults.trail_ns <= 0.0 ||
        !std::isfinite(defaults.delay_ns) || defaults.delay_ns < 0.0) {
      throw std::runtime_error(
          "Lecroy channel defaults require positive amplitude/width/edge "
          "times, a finite baseline, and a non-negative delay");
    }
  }
  for (const auto* defaults : {&config.lecroy_a_pulse_defaults,
                               &config.lecroy_b_pulse_defaults}) {
    if (!defaults->apply) continue;
    if (!std::isfinite(defaults->amplitude_v) || defaults->amplitude_v <= 0.0 ||
        !std::isfinite(defaults->baseline_v) ||
        !std::isfinite(defaults->width_ns) || defaults->width_ns <= 0.0 ||
        !std::isfinite(defaults->lead_ns) || defaults->lead_ns <= 0.0 ||
        !std::isfinite(defaults->trail_ns) || defaults->trail_ns <= 0.0 ||
        !std::isfinite(defaults->delay_ns) || defaults->delay_ns < 0.0) {
      throw std::runtime_error("invalid per-channel Lecroy pulse defaults");
    }
  }
  if (config.drain_quiet_ms <= 0.0 || config.drain_timeout_s <= 0.0) {
    throw std::runtime_error("drain settings must be positive");
  }
  if (config.primitive_gate_clocks < 0 ||
      config.primitive_gate_clocks > 255 ||
      config.latency_gate_clocks < 0 ||
      config.latency_gate_clocks > 255 ||
      config.external_gate_clocks < 3 ||
      config.external_gate_clocks > 255) {
    throw std::runtime_error("invalid L2 gate clock configuration");
  }
}

}  // namespace

BatchingScanConfig load_batching_scan_config(
    const std::filesystem::path& path,
    const std::filesystem::path& project_dir) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open batching-scan config: " +
                             path.string());
  }
  const auto document = nlohmann::json::parse(input);
  BatchingScanConfig config;

  const auto& scan = document.at("scan");
  config.frames_per_block = required_array<int>(scan, "frames_per_block");
  config.triggers_per_event =
      required_array<int>(scan, "triggers_per_event");
  config.lecroy_rates_hz = required_array<double>(scan, "lecroy_rates_hz");
  config.enabled_channels =
      scan.value("enabled_channels", std::vector<int>{});
  if (scan.contains("acquisition_schemes")) {
    config.acquisition_schemes.clear();
    for (const auto& value :
         required_array<std::string>(scan, "acquisition_schemes")) {
      config.acquisition_schemes.push_back(
          acquisition_scheme_from_string(value));
    }
  }
  config.repetitions = scan.at("repetitions").get<int>();

  const auto& acquisition = document.at("acquisition");
  config.duration_s = acquisition.at("duration_s").get<double>();
  config.post_start_settle_s =
      acquisition.value("post_start_settle_s", 1.0);
  config.startup_waveform_hits =
      acquisition.value("startup_waveform_hits", std::size_t{0});
  config.max_events = acquisition.at("max_events").get<int>();
  config.pipelined_decode = acquisition.value("pipelined_decode", false);
  config.raw_queue_capacity =
      acquisition.value("raw_queue_capacity", std::size_t{128});

  const auto& correlation = document.at("correlation");
  config.hit_offset_ns = correlation.at("hit_offset_ns").get<double>();
  config.auto_hit_offset = correlation.at("auto_hit_offset").get<bool>();
  config.pre_window_ns = correlation.at("pre_window_ns").get<double>();
  config.post_window_ns = correlation.at("post_window_ns").get<double>();

  const auto& gate = document.at("l2_gate");
  config.primitive_gate_clocks = gate.at("primitive_gate_clocks").get<int>();
  config.latency_gate_clocks = gate.at("latency_gate_clocks").get<int>();
  config.external_gate_clocks = gate.at("external_gate_clocks").get<int>();

  const auto& drain = document.at("drain");
  config.drain_quiet_ms = drain.at("quiet_ms").get<double>();
  config.drain_timeout_s = drain.at("timeout_s").get<double>();

  const auto& retry = document.at("retry");
  config.max_point_retries = retry.at("max_point_retries").get<int>();
  config.retry_delay_s = retry.at("delay_s").get<double>();

  config.output_root = resolve_project_path(
      document.at("output").at("root_directory").get<std::string>(),
      project_dir);
  config.hardware_config = resolve_project_path(
      document.at("probe").at("hardware_config").get<std::string>(),
      project_dir);
  const auto& probe = document.at("probe");
  const auto read_pulse_defaults = [](const nlohmann::json& defaults,
                                      LecroyPulseDefaults& output) {
    output.apply = true;
    output.amplitude_v = defaults.at("amplitude_v").get<double>();
    output.baseline_v = defaults.at("baseline_v").get<double>();
    output.width_ns = defaults.at("width_ns").get<double>();
    output.lead_ns = defaults.at("lead_ns").get<double>();
    output.trail_ns = defaults.at("trail_ns").get<double>();
    output.double_pulse_enabled =
        defaults.at("double_pulse_enabled").get<bool>();
    output.delay_ns = defaults.at("delay_ns").get<double>();
  };
  if (probe.contains("lecroy_channel_defaults"))
    read_pulse_defaults(probe.at("lecroy_channel_defaults"),
                        config.lecroy_pulse_defaults);
  if (probe.contains("lecroy_channel_a_defaults"))
    read_pulse_defaults(probe.at("lecroy_channel_a_defaults"),
                        config.lecroy_a_pulse_defaults);
  if (probe.contains("lecroy_channel_b_defaults"))
    read_pulse_defaults(probe.at("lecroy_channel_b_defaults"),
                        config.lecroy_b_pulse_defaults);

  validate(config);
  return config;
}

std::vector<BatchingScanPoint> build_batching_scan_points(
    const BatchingScanConfig& config) {
  std::vector<BatchingScanPoint> points;
  points.reserve(
      config.lecroy_rates_hz.size() * config.frames_per_block.size() *
      config.triggers_per_event.size() * config.acquisition_schemes.size() *
      static_cast<std::size_t>(config.repetitions));
  for (int repetition = 1; repetition <= config.repetitions; ++repetition) {
    for (const auto acquisition_scheme : config.acquisition_schemes) {
      for (const double rate : config.lecroy_rates_hz) {
        for (const int frames : config.frames_per_block) {
          for (const int triggers : config.triggers_per_event) {
            points.push_back(
                {acquisition_scheme, rate, frames, triggers, repetition});
          }
        }
      }
    }
  }
  return points;
}

std::string batching_scan_run_name(const BatchingScanPoint& point) {
  std::ostringstream output;
  output << "rate_" << std::setw(6) << std::setfill('0')
         << static_cast<long long>(std::llround(point.rate_hz))
         << "_frames_" << std::setw(2) << point.frames_per_block
         << "_triggers_" << std::setw(3) << point.triggers_per_event
         << "_rep_" << std::setw(2) << point.repetition
         << "_scheme_" << acquisition_scheme_name(point.acquisition_scheme);
  return output.str();
}

}  // namespace sampic::batching_scan
