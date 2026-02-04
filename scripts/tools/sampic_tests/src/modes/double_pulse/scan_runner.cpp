#include "sampic_tests/modes/double_pulse/scan_runner.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "sampic_tests/common/scan_types.h"
#include "sampic_tests/common/time_utils.h"
#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/manual_trigger_controller.h"
#include "sampic_tests/modes/double_pulse/sampic_session.h"

namespace sampic::double_pulse {
namespace {

using scan::SampleResult;

struct ChannelOccupancyCheck {
  std::vector<int> expected;
  std::vector<int> seen;
  std::vector<int> missing;
  std::vector<int> unexpected;
};

ChannelOccupancyCheck check_channel_occupancy(
    int board_index,
    const std::vector<int>& expected_channels,
    const std::map<std::pair<int, int>, std::size_t>& all_counts) {
  ChannelOccupancyCheck result;
  result.expected = expected_channels;
  std::sort(result.expected.begin(), result.expected.end());

  std::unordered_set<int> expected_set(expected_channels.begin(),
                                       expected_channels.end());
  std::unordered_map<int, int> counts;
  for (const auto& entry : all_counts) {
    const auto& key = entry.first;
    if (key.first != board_index) continue;
    counts[key.second] += static_cast<int>(entry.second);
  }

  for (const auto& entry : counts) {
    const int channel = entry.first;
    if (expected_set.count(channel) > 0) {
      result.seen.push_back(channel);
    } else {
      result.unexpected.push_back(channel);
    }
  }

  for (int channel : expected_channels) {
    if (counts.find(channel) == counts.end()) {
      result.missing.push_back(channel);
    }
  }

  std::sort(result.seen.begin(), result.seen.end());
  std::sort(result.missing.begin(), result.missing.end());
  std::sort(result.unexpected.begin(), result.unexpected.end());
  return result;
}

std::string format_channel_list(const std::vector<int>& channels) {
  if (channels.empty()) return "<none>";
  std::ostringstream oss;
  for (size_t i = 0; i < channels.size(); ++i) {
    if (i > 0) oss << " ";
    oss << channels[i];
  }
  return oss.str();
}

std::map<int, int> build_channel_counts(
    int board_index,
    const std::map<std::pair<int, int>, std::size_t>& all_counts) {
  std::map<int, int> board_counts;
  for (const auto& entry : all_counts) {
    const auto& key = entry.first;
    if (key.first != board_index) continue;
    board_counts[key.second] += static_cast<int>(entry.second);
  }
  return board_counts;
}

nlohmann::json channel_counts_to_json(const std::map<int, int>& counts) {
  nlohmann::json entries = nlohmann::json::array();
  for (const auto& entry : counts) {
    entries.push_back({{"channel", entry.first}, {"hits", entry.second}});
  }
  return entries;
}

std::optional<double> parse_double(const std::string& text) {
  try {
    size_t processed = 0;
    double value = std::stod(text, &processed);
    if (processed == 0) return std::nullopt;
    return value;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

double read_frequency_hz(sampic::lecroy::LecroyClient& lecroy, double fallback) {
  try {
    const auto resp = lecroy.Query("FREQ?");
    if (auto parsed = parse_double(resp)) {
      return *parsed;
    }
  } catch (const std::exception&) {
  }
  return fallback;
}

double read_delay_ns(sampic::lecroy::LecroyClient& lecroy,
                     const std::string& channel,
                     double fallback_ns) {
  try {
    const auto resp = lecroy.Query(channel + ":DEL?");
    if (auto parsed = parse_double(resp)) {
      return *parsed * 1e9;
    }
  } catch (const std::exception&) {
  }
  return fallback_ns;
}

std::string primary_lecroy_channel(const sampic::lecroy::LecroyConfig& cfg) {
  if (!cfg.channels.empty()) return cfg.channels.front();
  return cfg.channel.channel;
}

nlohmann::json sample_stats_to_json(const SampleResult& sample) {
  nlohmann::json j;
  j["success"] = sample.success;
  j["duration_s"] = sample.stats.duration_s;
  j["events"] = sample.stats.events;
  j["total_hits"] = sample.stats.total_hits;
  j["total_bytes"] = sample.stats.total_bytes;
  if (sample.stats.duration_s > 0.0) {
    j["events_per_second"] = sample.stats.events / sample.stats.duration_s;
    j["data_mb_per_s"] =
        (static_cast<double>(sample.stats.total_bytes) / (1024.0 * 1024.0)) /
        sample.stats.duration_s;
  } else {
    j["events_per_second"] = nlohmann::json();
    j["data_mb_per_s"] = nlohmann::json();
  }
  if (sample.stats.events > 0) {
    j["hits_per_event"] =
        static_cast<double>(sample.stats.total_hits) /
        static_cast<double>(sample.stats.events);
  } else {
    j["hits_per_event"] = nlohmann::json();
  }
  j["retries"] = sample.stats.retries;
  j["decode_errors"] = sample.stats.decode_errors;
  j["max_loop_hits"] = sample.stats.max_loop_hits;
  j["hit_timestamp_separation"] = sample.stats.hit_separation.to_json();
  j["errors"] = sample.stats.error_messages;
  if (!sample.hits.empty()) {
    j["hit_count"] = sample.hits.size();
  } else {
    j["hit_count"] = 0;
  }
  return j;
}

scan::SampleStats aggregate_stats(const std::vector<SampleResult>& samples) {
  scan::SampleStats agg;
  for (const auto& sample : samples) {
    agg.events += sample.stats.events;
    agg.total_hits += sample.stats.total_hits;
    agg.total_bytes += sample.stats.total_bytes;
    agg.retries += sample.stats.retries;
    agg.decode_errors += sample.stats.decode_errors;
    agg.max_loop_hits += sample.stats.max_loop_hits;
    agg.duration_s += sample.stats.duration_s;
    agg.hit_separation.merge(sample.stats.hit_separation);
    for (const auto& entry : sample.stats.channel_hit_counts) {
      agg.channel_hit_counts[entry.first] += entry.second;
    }
    for (const auto& err : sample.stats.error_messages) {
      agg.record_error(err);
    }
  }
  return agg;
}

double estimate_hit_rate_hz(const std::vector<scan::HitRecord>& hits) {
  if (hits.size() < 2) return 0.0;
  auto minmax = std::minmax_element(
      hits.begin(), hits.end(),
      [](const auto& a, const auto& b) { return a.first_cell_ts_ns < b.first_cell_ts_ns; });
  const double span_ns = minmax.second->first_cell_ts_ns - minmax.first->first_cell_ts_ns;
  if (span_ns <= 0.0) return 0.0;
  const double span_s = span_ns * 1e-9;
  if (span_s <= 0.0) return 0.0;
  return static_cast<double>(hits.size() - 1) / span_s;
}

SampleResult collect_sample(SampicSession& session,
                            const DoublePulseConfig& cfg,
                            volatile std::sig_atomic_t* stop_flag,
                            bool capture_hits = false) {
  SampleResult result;
  try {
    if (capture_hits) {
      result.stats = session.acquire_sample(cfg.readout, cfg.timing.sample_duration_s,
                                            stop_flag, true, &result.hits);
    } else {
      result.stats = session.acquire_sample(cfg.readout, cfg.timing.sample_duration_s,
                                            stop_flag, false, nullptr);
    }
    result.success = !(stop_flag && *stop_flag);
  } catch (const std::exception& ex) {
    result.stats.record_error(std::string("Sample failed: ") + ex.what());
    result.success = false;
  }
  return result;
}

std::unordered_set<std::string> load_completed(const std::filesystem::path& path) {
  std::unordered_set<std::string> completed;
  if (!std::filesystem::exists(path)) return completed;
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Unable to read results file: " + path.string());
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    const auto record = nlohmann::json::parse(line, nullptr, true, true);
    if (!record.contains("status")) continue;
    if (record.at("status").get<std::string>() != "complete") continue;
    const auto key = make_combo_key_from_json(record);
    if (!key.empty()) completed.insert(key);
  }
  return completed;
}

nlohmann::json build_record(const DoublePulseConfig& cfg,
                            const ParameterCombination& combo,
                            double best_delay_ns,
                            double current_delay_ns,
                            double hit_rate_hz,
                            double event_rate_hz,
                            double expected_hit_rate_hz,
                            double hit_ratio_vs_freq,
                            double ratio_vs_freq,
                            double search_min_ns,
                            double search_max_ns,
                            const std::vector<SampleResult>& samples,
                            const std::optional<AppliedSettings>& readback,
                            const std::optional<std::string>& readback_error,
                            const scan::RunStatus& run_status,
                            const std::string& status,
                            const std::string& key,
                            double wall_seconds,
                            const nlohmann::json& history) {
  nlohmann::json record;
  record["timestamp"] = common::TimeUtils::Iso8601Now();
  record["status"] = status;
  record["combo_key"] = key;
  record["wall_time_s"] = wall_seconds;
  record["parameters"] = {
      {"lecroy_frequency_hz", combo.lecroy_frequency_hz},
      {"digitizer_rate_mhz", combo.digitizer_rate_mhz},
      {"auto_conversion", combo.auto_conversion},
      {"lecroy_amplitude_v", combo.lecroy_amplitude_v},
      {"threshold_volts", combo.threshold_volts},
      {"channel_label", combo.channel_label},
      {"best_delay_ns", best_delay_ns},
      {"current_delay_ns", current_delay_ns},
      {"search_min_ns", search_min_ns},
      {"search_max_ns", search_max_ns},
      {"channel_ids", combo.channels},
      {"channel_count", combo.channels.size()},
      {"board_index", cfg.scan.board_index}};

  const auto agg = aggregate_stats(samples);
  nlohmann::json sample_array = nlohmann::json::array();
  for (const auto& sample : samples) {
    sample_array.push_back(sample_stats_to_json(sample));
  }
  record["samples"] = sample_array;
  record["aggregate"] = {
      {"events", agg.events},
      {"total_hits", agg.total_hits},
      {"total_bytes", agg.total_bytes},
      {"duration_s", agg.duration_s},
      {"retries", agg.retries},
      {"decode_errors", agg.decode_errors},
      {"max_loop_hits", agg.max_loop_hits},
      {"hit_timestamp_separation", agg.hit_separation.to_json()}};
  if (agg.duration_s > 0.0) {
    record["aggregate"]["events_per_second"] = agg.events / agg.duration_s;
    record["aggregate"]["data_mb_per_s"] =
        (static_cast<double>(agg.total_bytes) / (1024.0 * 1024.0)) /
        agg.duration_s;
  }
  if (agg.events > 0) {
    record["aggregate"]["hits_per_event"] =
        static_cast<double>(agg.total_hits) / static_cast<double>(agg.events);
  }

  record["run"] = {
      {"started", run_status.run_started},
      {"start_attempts", run_status.start_attempts},
      {"start_errors", run_status.start_errors}};

  if (readback.has_value()) {
    record["readback"] = {
        {"sampling_frequency_mhz", readback->sampling_frequency_mhz},
        {"use_external_clock", readback->use_external_clock},
        {"enabled_channel_count", readback->enabled_channels.size()},
        {"enabled_channels", readback->enabled_channels}};
  } else {
    record["readback"] = nlohmann::json();
  }
  if (readback_error.has_value()) {
    record["readback_error"] = *readback_error;
  } else {
    record["readback_error"] = nlohmann::json();
  }

  record["timing_config"] = {
      {"sample_duration_s", cfg.timing.sample_duration_s},
      {"samples_per_combo", cfg.timing.samples_per_combo},
      {"cooldown_between_combos_s", cfg.timing.cooldown_between_combos_s}};
  record["readout_config"] = {
      {"prepare_interval", cfg.readout.prepare_interval},
      {"max_loops", cfg.readout.max_loops},
      {"retry_sleep_us", cfg.readout.retry_sleep_us}};
  record["start_retry"] = {
      {"max_attempts", cfg.start_retry.max_attempts},
      {"initial_delay_s", cfg.start_retry.initial_delay_s},
      {"backoff", cfg.start_retry.backoff}};
  record["lecroy"] = {
      {"channels", cfg.lecroy.channels.empty() ? std::vector<std::string>{cfg.lecroy.channel.channel}
                                               : cfg.lecroy.channels},
      {"frequency_hz", combo.lecroy_frequency_hz},
      {"amplitude_v", combo.lecroy_amplitude_v},
      {"baseline_v", cfg.lecroy.channel.baseline_v},
      {"width_ns", cfg.lecroy.channel.width_ns},
      {"double_pulse_enabled", cfg.lecroy.channel.double_pulse_enabled},
      {"manual_trigger", cfg.lecroy.manual_trigger}};
  record["search"] = {
      {"best_delay_ns", best_delay_ns},
      {"current_delay_ns", current_delay_ns},
      {"hit_rate_hz", hit_rate_hz},
      {"event_rate_hz", event_rate_hz},
      {"expected_hit_rate_hz", expected_hit_rate_hz},
      {"hit_ratio_vs_frequency", hit_ratio_vs_freq},
      {"ratio_vs_frequency", ratio_vs_freq},
      {"ratio_threshold", cfg.search.ratio_threshold},
      {"tolerance_ns", cfg.search.tolerance_ns},
      {"search_min_ns", search_min_ns},
      {"search_max_ns", search_max_ns},
      {"history", history}};
  return record;
}

void append_record(const std::filesystem::path& path, const nlohmann::json& record) {
  if (auto parent = path.parent_path(); !parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  std::ofstream out(path, std::ios::app);
  if (!out) {
    throw std::runtime_error("Unable to open results file: " + path.string());
  }
  out << record.dump() << '\n';
}

}  // namespace

DoublePulseScanRunner::DoublePulseScanRunner(DoublePulseConfig config,
                                             volatile std::sig_atomic_t* stop_flag)
    : config_(std::move(config)), stop_flag_(stop_flag) {}

int DoublePulseScanRunner::run(bool debug_mode) {
  auto combos = build_parameter_space(config_.scan);
  auto completed = load_completed(config_.output.results_path);
  if (debug_mode && combos.size() > 1) {
    combos.resize(1);
    std::cout << "Debug mode: running only the first parameter combination.\n";
  }

  auto stop_requested = [&]() -> bool { return stop_flag_ && *stop_flag_; };

  sampic::lecroy::LecroyClient lecroy;
  lecroy.Configure(config_.lecroy);
  std::unique_ptr<sampic::lecroy::ManualTriggerController> manual_trigger;
  if (config_.lecroy.manual_trigger && config_.lecroy.manual_trigger_interval_s > 0.0) {
    manual_trigger = std::make_unique<sampic::lecroy::ManualTriggerController>(
        &lecroy, config_.lecroy.manual_trigger_interval_s, stop_flag_);
  }

  auto make_session = [&]() {
    return std::make_unique<SampicSession>(config_.connection, config_.external_trigger);
  };
  std::unique_ptr<SampicSession> session;
  auto ensure_session = [&]() -> SampicSession& {
    if (!session) {
      session = make_session();
    }
    return *session;
  };

  std::cout << "Parameter combinations: " << combos.size() << "\n";
  auto report_sample = [&](const SampleResult& sample, int sample_idx, int total_samples) {
    if (!debug_mode) return;
    std::cout << "    sample ";
    if (sample_idx >= 0) {
      std::cout << (sample_idx + 1) << "/" << total_samples;
    } else {
      std::cout << "(setup failure)";
    }
    std::cout << " success=" << std::boolalpha << sample.success
              << " events=" << sample.stats.events
              << " hits=" << sample.stats.total_hits
              << " errors=" << sample.stats.error_messages.size() << "\n";
    for (const auto& err : sample.stats.error_messages) {
      std::cout << "      error: " << err << "\n";
    }
    std::cout << std::noboolalpha;
  };

  const std::size_t estimated_measurements =
      std::max<std::size_t>(1, combos.size() * config_.search.max_iterations);
  std::size_t completed_measurements = 0;
  const auto scan_start = std::chrono::steady_clock::now();

  std::size_t combo_index = 0;
  for (auto combo : combos) {
    if (stop_requested()) {
      std::cout << "Stop requested. Exiting.\n";
      break;
    }
    ++combo_index;
    const auto key = make_combo_key(combo, config_.scan.board_index);
    if (completed.count(key) > 0) {
      std::cout << "[skip] (" << combo_index << "/" << combos.size()
                << ") already recorded: " << key << "\n";
      continue;
    }

    const double target_freq_hz = combo.lecroy_frequency_hz > 0.0
                                       ? combo.lecroy_frequency_hz
                                       : config_.lecroy.frequency_hz;
    lecroy.SetFrequency(target_freq_hz);
    if (combo.lecroy_amplitude_v > 0.0) {
      lecroy.SetAmplitude(combo.lecroy_amplitude_v);
    }
    const double applied_freq_hz = read_frequency_hz(lecroy, target_freq_hz);
    combo.lecroy_frequency_hz = applied_freq_hz;
    if (std::abs(applied_freq_hz - target_freq_hz) > std::max(1.0, target_freq_hz) * 0.01) {
      std::cout << "    Warning: Lecroy frequency readback " << applied_freq_hz
                << " Hz differs from requested " << target_freq_hz << " Hz\n";
    }

    std::cout << "[run ] (" << combo_index << "/" << combos.size()
              << ") freq=" << applied_freq_hz << "Hz"
              << " digitizer=" << combo.digitizer_rate_mhz << "MHz"
              << " auto_conv=" << (combo.auto_conversion ? "on" : "off")
              << " amp=" << combo.lecroy_amplitude_v << "V"
              << " thr=" << combo.threshold_volts << "V"
              << " channels=" << combo.channels.size();
    if (!combo.channel_label.empty()) {
      std::cout << " (" << combo.channel_label << ")";
    }
    std::cout << "\n";
    std::cout << "    Lecroy frequency readback=" << applied_freq_hz << " Hz\n";
    const std::string channel_name = primary_lecroy_channel(config_.lecroy);

    const auto combo_start = std::chrono::steady_clock::now();
    bool combo_failed = false;
    bool found_threshold = false;
    scan::RunStatus best_run_status;
    std::optional<AppliedSettings> readback_settings;
    std::optional<std::string> readback_error;
    std::vector<SampleResult> best_samples;
    double best_delay_ns = config_.search.max_ns;
    double best_hit_rate_hz = 0.0;
    double best_event_rate_hz = 0.0;
    double best_hit_ratio = 0.0;
    double best_event_ratio = 0.0;
    std::map<int, int> best_channel_counts;
    nlohmann::json history = nlohmann::json::array();

    SampicSession* active_session = nullptr;
    const int max_session_attempts = 2;
    std::string config_error;
    for (int attempt = 0; attempt < max_session_attempts && !active_session; ++attempt) {
      try {
        auto& sess = ensure_session();
        sess.configure_for_combo(combo, config_.scan.board_index, combo.channels);
        active_session = &sess;
      } catch (const std::exception& ex) {
        config_error = ex.what();
        if (attempt + 1 >= max_session_attempts) {
          combo_failed = true;
          std::vector<SampleResult> failure_samples;
          SampleResult failure;
          failure.stats.record_error(std::string("Configuration failed: ") + config_error);
          failure.success = false;
          failure_samples.push_back(failure);
          report_sample(failure_samples.back(), -1, config_.timing.samples_per_combo);
          best_samples = failure_samples;
        } else {
          std::cerr << "Configuration failed (" << config_error
                    << "). Resetting session...\n";
        }
        session.reset();
      }
    }

    if (active_session) {
      try {
        readback_settings = active_session->readback_settings(config_.scan.board_index);
      } catch (const std::exception& ex) {
        readback_error = ex.what();
        if (debug_mode) {
          std::cout << "    readback error: " << *readback_error << "\n";
        }
      }
    }

    double low = config_.search.min_ns;
    double high = config_.search.max_ns;
    double delay_ns = std::clamp(config_.search.start_ns, low, high);
    int iteration = 0;
    std::vector<SampleResult> last_samples;
    scan::RunStatus last_run_status;
    double last_hit_rate = 0.0;
    double last_event_rate = 0.0;
    double last_hit_ratio = 0.0;
    double last_event_ratio = 0.0;
    std::map<int, int> last_channel_counts;

    while (!combo_failed && !stop_requested() && iteration < config_.search.max_iterations) {
      ++iteration;
      lecroy.SetDoublePulseDelay(delay_ns);
      const double applied_delay_ns = read_delay_ns(lecroy, channel_name, delay_ns);
      std::cout << "    Requested delay=" << delay_ns << " ns, readback="
                << applied_delay_ns << " ns\n";

      std::vector<SampleResult> samples;
      samples.reserve(static_cast<std::size_t>(config_.timing.samples_per_combo));
      scan::RunStatus run_status;
      std::vector<sampic::scan::HitRecord> aggregated_hits;

      std::vector<std::string> start_errors;
      int start_attempts = 0;
      bool iteration_failed = false;
      if (!active_session ||
          !active_session->start_run_with_retry(config_.start_retry, start_attempts, start_errors)) {
        iteration_failed = true;
        run_status.run_started = false;
        run_status.start_attempts = start_attempts;
        run_status.start_errors = start_errors;
        SampleResult failure;
        failure.stats.record_error("Run failed to start after retry budget exhausted.");
        failure.success = false;
        samples.push_back(failure);
      } else {
        run_status.run_started = true;
        run_status.start_attempts = start_attempts;
        run_status.start_errors = start_errors;
        sampic::lecroy::ManualTriggerGuard trigger_guard(manual_trigger.get());
        for (int sample_idx = 0; sample_idx < config_.timing.samples_per_combo; ++sample_idx) {
          if (stop_requested()) break;
          auto sample = collect_sample(*active_session, config_, stop_flag_, true);
          aggregated_hits.insert(aggregated_hits.end(), sample.hits.begin(), sample.hits.end());
          samples.push_back(sample);
          if (!sample.success) {
            iteration_failed = true;
          }
          report_sample(samples.back(), sample_idx, config_.timing.samples_per_combo);
          if (stop_requested()) break;
        }
      }
      if (active_session) {
        active_session->stop_run();
      }

      last_samples = samples;
      last_run_status = run_status;

      const auto agg_stats = aggregate_stats(samples);
      const double event_rate_hz =
          (agg_stats.duration_s > 0.0) ? (agg_stats.events / agg_stats.duration_s) : 0.0;
      const auto occupancy =
          check_channel_occupancy(config_.scan.board_index, combo.channels,
                                  agg_stats.channel_hit_counts);
      const auto channel_counts =
          build_channel_counts(config_.scan.board_index, agg_stats.channel_hit_counts);
      std::size_t board_total_hits = 0;
      for (const auto& entry : channel_counts) {
        board_total_hits += static_cast<std::size_t>(entry.second);
      }
      const double hit_rate_hz =
          (agg_stats.duration_s > 0.0)
              ? (static_cast<double>(board_total_hits) / agg_stats.duration_s)
              : 0.0;
      const double expected_hit_rate_hz =
          (applied_freq_hz > 0.0)
              ? (applied_freq_hz * static_cast<double>(combo.channels.size()))
              : 0.0;
      const double hit_ratio =
          expected_hit_rate_hz > 0.0 ? hit_rate_hz / expected_hit_rate_hz : 0.0;
      const double ratio = applied_freq_hz > 0.0 ? event_rate_hz / applied_freq_hz : 0.0;
      if (!occupancy.missing.empty() || !occupancy.unexpected.empty()) {
        std::cout << "    Channel occupancy check (board " << config_.scan.board_index
                  << "): expected=" << format_channel_list(occupancy.expected)
                  << " seen=" << format_channel_list(occupancy.seen)
                  << " missing=" << format_channel_list(occupancy.missing)
                  << " unexpected=" << format_channel_list(occupancy.unexpected) << "\n";
      }
      bool double_detected =
          !iteration_failed && hit_ratio >= config_.search.ratio_threshold;

      double new_low = low;
      double new_high = high;
      if (double_detected) {
        new_high = delay_ns;
        found_threshold = true;
        best_delay_ns = applied_delay_ns;
        best_samples = samples;
        best_run_status = run_status;
        best_hit_rate_hz = hit_rate_hz;
        best_event_rate_hz = event_rate_hz;
        best_hit_ratio = hit_ratio;
        best_event_ratio = ratio;
        best_channel_counts = channel_counts;
      } else {
        new_low = delay_ns;
        if (!found_threshold) {
          best_delay_ns = applied_delay_ns;
          best_samples = samples;
          best_run_status = run_status;
          best_hit_rate_hz = hit_rate_hz;
          best_event_rate_hz = event_rate_hz;
          best_hit_ratio = hit_ratio;
          best_event_ratio = ratio;
          best_channel_counts = channel_counts;
        }
      }

      last_hit_rate = hit_rate_hz;
      last_event_rate = event_rate_hz;
      last_hit_ratio = hit_ratio;
      last_event_ratio = ratio;
      last_channel_counts = channel_counts;

      history.push_back({
          {"iteration", iteration},
          {"target_delay_ns", delay_ns},
          {"applied_delay_ns", applied_delay_ns},
          {"hit_rate_hz", hit_rate_hz},
          {"event_rate_hz", event_rate_hz},
          {"hit_ratio_vs_frequency", hit_ratio},
          {"ratio_vs_frequency", ratio},
          {"channel_hit_counts", channel_counts_to_json(channel_counts)},
          {"expected_channels", occupancy.expected},
          {"seen_channels", occupancy.seen},
          {"missing_channels", occupancy.missing},
          {"unexpected_channels", occupancy.unexpected},
          {"double_detected", double_detected},
          {"frequency_hz", applied_freq_hz},
          {"expected_hit_rate_hz", expected_hit_rate_hz},
          {"events", agg_stats.events},
          {"total_hits", agg_stats.total_hits},
          {"duration_s", agg_stats.duration_s},
          {"errors", agg_stats.error_messages},
          {"search_min_ns", new_low},
          {"search_max_ns", new_high}});

      ++completed_measurements;
      const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - scan_start).count();
      const double avg = elapsed / static_cast<double>(completed_measurements);
      const double remaining =
          std::max(0.0, (estimated_measurements - completed_measurements) * avg);
      std::cout << "    [iter " << iteration << "/" << config_.search.max_iterations
                << "] delay=" << delay_ns << " ns, hits=" << board_total_hits
                << " hit_ratio=" << std::fixed << std::setprecision(3) << hit_ratio
                << (double_detected ? " (double)" : " (single)")
                << " | progress " << completed_measurements << "/"
                << estimated_measurements << ", ETA ~"
                << std::setprecision(1) << remaining / 60.0 << " min\n";
      if (!channel_counts.empty()) {
        std::cout << "    Channels seen (board " << config_.scan.board_index << "):";
        for (const auto& entry : channel_counts) {
          std::cout << " " << entry.first << ":" << entry.second;
        }
        std::cout << "\n";
      }

      const auto iteration_record =
          build_record(config_, combo, best_delay_ns, applied_delay_ns, hit_rate_hz,
                       event_rate_hz, expected_hit_rate_hz, hit_ratio, ratio,
                       new_low, new_high, samples, readback_settings, readback_error,
                       run_status,
                       double_detected ? "iteration_double" : "iteration_single", key,
                       std::chrono::duration<double>(std::chrono::steady_clock::now() - combo_start)
                           .count(),
                       history);
      append_record(config_.output.results_path, iteration_record);
      low = new_low;
      high = new_high;

      if (iteration_failed) {
        combo_failed = true;
        break;
      }

      if (high - low <= config_.search.tolerance_ns) {
        break;
      }
      delay_ns = 0.5 * (low + high);
    }

    if (!found_threshold && !combo_failed) {
      best_samples = last_samples;
      best_run_status = last_run_status;
      best_hit_rate_hz = last_hit_rate;
      best_event_rate_hz = last_event_rate;
      best_hit_ratio = last_hit_ratio;
      best_event_ratio = last_event_ratio;
      best_channel_counts = last_channel_counts;
    }

    if (config_.timing.cooldown_between_combos_s > 0.0 && !stop_requested()) {
      std::this_thread::sleep_for(
          std::chrono::duration<double>(config_.timing.cooldown_between_combos_s));
    }

    const auto combo_end = std::chrono::steady_clock::now();
    const double wall_seconds = std::chrono::duration<double>(combo_end - combo_start).count();
    const std::string status = stop_requested()
                                   ? "aborted"
                                   : (combo_failed ? "failed"
                                                  : (found_threshold ? "complete"
                                                                     : "threshold_not_met"));

    const auto& samples_for_record = best_samples.empty() ? last_samples : best_samples;
    const auto& run_status_for_record = best_samples.empty() ? last_run_status : best_run_status;
    const double hit_rate_for_record = best_samples.empty() ? last_hit_rate : best_hit_rate_hz;
    const double event_rate_for_record =
        best_samples.empty() ? last_event_rate : best_event_rate_hz;
    const double hit_ratio_for_record =
        best_samples.empty() ? last_hit_ratio : best_hit_ratio;
    const double ratio_for_record =
        best_samples.empty() ? last_event_ratio : best_event_ratio;

    combo.pulse_separation_ns = best_delay_ns;
    const double expected_hit_rate_for_record =
        (applied_freq_hz > 0.0)
            ? (applied_freq_hz * static_cast<double>(combo.channels.size()))
            : 0.0;
    auto record = build_record(config_, combo, best_delay_ns, best_delay_ns,
                               hit_rate_for_record, event_rate_for_record,
                               expected_hit_rate_for_record, hit_ratio_for_record,
                               ratio_for_record, low, high, samples_for_record,
                               readback_settings, readback_error,
                               run_status_for_record, status, key, wall_seconds,
                               history);
    record["channel_summary"] = channel_counts_to_json(best_channel_counts);
    append_record(config_.output.results_path, record);
    if (status == "complete") {
      completed.insert(key);
    }
    std::cout << "  \u2192 status: " << status << ", duration=" << std::fixed
              << std::setprecision(1) << wall_seconds << "s\n";
    if (!best_channel_counts.empty()) {
      std::cout << "  Channels seen (board " << config_.scan.board_index << "):";
      for (const auto& entry : best_channel_counts) {
        std::cout << " " << entry.first << ":" << entry.second;
      }
      std::cout << "\n";
    }
  }

  return stop_requested() ? 1 : 0;
}

}  // namespace sampic::double_pulse
