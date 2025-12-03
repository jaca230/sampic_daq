#include "sampic_tests/modes/double_pulse/scan_runner.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
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
                            double hit_rate_hz,
                            double ratio_vs_freq,
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
      {"best_delay_ns", best_delay_ns},
      {"channel_ids", cfg.scan.channels},
      {"channel_count", cfg.scan.channels.size()},
      {"channel_label", "manual_selection"},
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
      {"channel", cfg.lecroy.channel.channel},
      {"frequency_hz", combo.lecroy_frequency_hz},
      {"amplitude_v", cfg.lecroy.channel.amplitude_v},
      {"baseline_v", cfg.lecroy.channel.baseline_v},
      {"width_ns", cfg.lecroy.channel.width_ns},
      {"double_pulse_enabled", cfg.lecroy.channel.double_pulse_enabled},
      {"manual_trigger", cfg.lecroy.manual_trigger}};
  record["search"] = {
      {"best_delay_ns", best_delay_ns},
      {"hit_rate_hz", hit_rate_hz},
      {"ratio_vs_frequency", ratio_vs_freq},
      {"ratio_threshold", cfg.search.ratio_threshold},
      {"tolerance_ns", cfg.search.tolerance_ns},
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
    const double applied_freq_hz = read_frequency_hz(lecroy, target_freq_hz);
    combo.lecroy_frequency_hz = applied_freq_hz;
    if (std::abs(applied_freq_hz - target_freq_hz) > std::max(1.0, target_freq_hz) * 0.01) {
      std::cout << "    Warning: Lecroy frequency readback " << applied_freq_hz
                << " Hz differs from requested " << target_freq_hz << " Hz\n";
    }

    std::cout << "[run ] (" << combo_index << "/" << combos.size()
              << ") freq=" << applied_freq_hz << "Hz"
              << " digitizer=" << combo.digitizer_rate_mhz << "MHz"
              << " channels=" << config_.scan.channels.size() << "\n";
    std::cout << "    Lecroy frequency readback=" << applied_freq_hz << " Hz\n";
    const std::string channel_name = config_.lecroy.channel.channel;

    const auto combo_start = std::chrono::steady_clock::now();
    bool combo_failed = false;
    bool found_threshold = false;
    scan::RunStatus best_run_status;
    std::optional<AppliedSettings> readback_settings;
    std::optional<std::string> readback_error;
    std::vector<SampleResult> best_samples;
    double best_delay_ns = config_.search.max_ns;
    double best_hit_rate_hz = 0.0;
    double best_ratio = 0.0;
    nlohmann::json history = nlohmann::json::array();

    SampicSession* active_session = nullptr;
    const int max_session_attempts = 2;
    std::string config_error;
    for (int attempt = 0; attempt < max_session_attempts && !active_session; ++attempt) {
      try {
        auto& sess = ensure_session();
        sess.configure_for_combo(combo, config_.scan.board_index, config_.scan.channels);
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
    double last_ratio = 0.0;

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
      double hit_rate_hz = estimate_hit_rate_hz(aggregated_hits);
      if (hit_rate_hz <= 0.0 && agg_stats.duration_s > 0.0) {
        hit_rate_hz = agg_stats.total_hits / agg_stats.duration_s;
      }
      const double ratio = applied_freq_hz > 0.0 ? hit_rate_hz / applied_freq_hz : 0.0;
      bool double_detected = !iteration_failed && ratio >= config_.search.ratio_threshold;

      last_hit_rate = hit_rate_hz;
      last_ratio = ratio;

      history.push_back({
          {"iteration", iteration},
          {"target_delay_ns", delay_ns},
          {"applied_delay_ns", applied_delay_ns},
          {"hit_rate_hz", hit_rate_hz},
          {"ratio_vs_frequency", ratio},
          {"double_detected", double_detected},
          {"frequency_hz", applied_freq_hz},
          {"events", agg_stats.events},
          {"total_hits", agg_stats.total_hits},
          {"duration_s", agg_stats.duration_s},
          {"errors", agg_stats.error_messages}});

      ++completed_measurements;
      const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - scan_start).count();
      const double avg = elapsed / static_cast<double>(completed_measurements);
      const double remaining =
          std::max(0.0, (estimated_measurements - completed_measurements) * avg);
      std::cout << "    [iter " << iteration << "/" << config_.search.max_iterations
                << "] delay=" << delay_ns << " ns, hits=" << agg_stats.total_hits
                << " ratio=" << std::fixed << std::setprecision(3) << ratio
                << (double_detected ? " (double)" : " (single)")
                << " | progress " << completed_measurements << "/"
                << estimated_measurements << ", ETA ~"
                << std::setprecision(1) << remaining / 60.0 << " min\n";

      const auto iteration_record = build_record(
          config_, combo, applied_delay_ns, hit_rate_hz, ratio, samples, readback_settings,
          readback_error, run_status, double_detected ? "iteration_double" : "iteration_single",
          key, std::chrono::duration<double>(std::chrono::steady_clock::now() - combo_start).count(),
          history);
      append_record(config_.output.results_path, iteration_record);

      if (double_detected) {
        found_threshold = true;
        best_delay_ns = applied_delay_ns;
        best_samples = samples;
        best_run_status = run_status;
        best_hit_rate_hz = hit_rate_hz;
        best_ratio = ratio;
        high = delay_ns;
      } else {
        low = delay_ns;
        if (!found_threshold) {
          best_delay_ns = applied_delay_ns;
          best_samples = samples;
          best_run_status = run_status;
          best_hit_rate_hz = hit_rate_hz;
          best_ratio = ratio;
        }
      }

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
      best_ratio = last_ratio;
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
    const double ratio_for_record = best_samples.empty() ? last_ratio : best_ratio;

    combo.pulse_separation_ns = best_delay_ns;
    const auto record = build_record(config_, combo, best_delay_ns, hit_rate_for_record,
                                     ratio_for_record, samples_for_record, readback_settings,
                                     readback_error, run_status_for_record, status, key,
                                     wall_seconds, history);
    append_record(config_.output.results_path, record);
    if (status == "complete") {
      completed.insert(key);
    }
    std::cout << "  \u2192 status: " << status << ", duration=" << std::fixed
              << std::setprecision(1) << wall_seconds << "s\n";
  }

  return stop_requested() ? 1 : 0;
}

}  // namespace sampic::double_pulse
