#include "sampic_tests/modes/double_pulse/scan_runner.h"

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
  return j;
}

struct ComboAggregate {
  std::size_t events = 0;
  std::size_t total_hits = 0;
  std::size_t total_bytes = 0;
  double total_duration_s = 0.0;
  std::size_t retries = 0;
  std::size_t decode_errors = 0;
  std::size_t max_loop_hits = 0;
  scan::SeparationAccumulator separation;

  void accumulate(const SampleResult& sample) {
    events += sample.stats.events;
    total_hits += sample.stats.total_hits;
    total_bytes += sample.stats.total_bytes;
    total_duration_s += sample.stats.duration_s;
    retries += sample.stats.retries;
    decode_errors += sample.stats.decode_errors;
    max_loop_hits += sample.stats.max_loop_hits;
    separation.merge(sample.stats.hit_separation);
  }
};

SampleResult collect_sample(SampicSession& session,
                            const DoublePulseConfig& cfg,
                            volatile std::sig_atomic_t* stop_flag) {
  SampleResult result;
  try {
    result.stats = session.acquire_sample(cfg.readout, cfg.timing.sample_duration_s, stop_flag);
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
                            const std::vector<SampleResult>& samples,
                            const std::optional<AppliedSettings>& readback,
                            const std::optional<std::string>& readback_error,
                            const scan::RunStatus& run_status,
                            const std::string& status,
                            const std::string& key,
                            double wall_seconds) {
  nlohmann::json record;
  record["timestamp"] = common::TimeUtils::Iso8601Now();
  record["status"] = status;
  record["combo_key"] = key;
  record["wall_time_s"] = wall_seconds;
  record["parameters"] = {
      {"double_pulse_delay_ns", combo.pulse_separation_ns},
      {"digitizer_rate_mhz", combo.digitizer_rate_mhz},
      {"channel_ids", cfg.scan.channels},
      {"channel_count", cfg.scan.channels.size()},
      {"channel_label", "manual_selection"},
      {"board_index", cfg.scan.board_index}};

  ComboAggregate agg;
  nlohmann::json sample_array = nlohmann::json::array();
  for (const auto& sample : samples) {
    agg.accumulate(sample);
    sample_array.push_back(sample_stats_to_json(sample));
  }
  record["samples"] = sample_array;
  record["aggregate"] = {
      {"events", agg.events},
      {"total_hits", agg.total_hits},
      {"total_bytes", agg.total_bytes},
      {"duration_s", agg.total_duration_s},
      {"retries", agg.retries},
      {"decode_errors", agg.decode_errors},
      {"max_loop_hits", agg.max_loop_hits},
      {"hit_timestamp_separation", agg.separation.to_json()}};
  if (agg.total_duration_s > 0.0) {
    record["aggregate"]["events_per_second"] = agg.events / agg.total_duration_s;
    record["aggregate"]["data_mb_per_s"] =
        (static_cast<double>(agg.total_bytes) / (1024.0 * 1024.0)) /
        agg.total_duration_s;
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
      {"frequency_hz", cfg.lecroy.frequency_hz},
      {"amplitude_v", cfg.lecroy.channel.amplitude_v},
      {"baseline_v", cfg.lecroy.channel.baseline_v},
      {"width_ns", cfg.lecroy.channel.width_ns},
      {"double_pulse_enabled", cfg.lecroy.channel.double_pulse_enabled},
      {"manual_trigger", cfg.lecroy.manual_trigger}};
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

  std::size_t combo_index = 0;
  for (const auto& combo : combos) {
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

    std::cout << "[run ] (" << combo_index << "/" << combos.size()
              << ") separation=" << combo.pulse_separation_ns << "ns"
              << " digitizer=" << combo.digitizer_rate_mhz << "MHz"
              << " channels=" << config_.scan.channels.size() << "\n";

    const auto combo_start = std::chrono::steady_clock::now();
    bool combo_failed = false;
    std::vector<SampleResult> samples;
    samples.reserve(static_cast<std::size_t>(config_.timing.samples_per_combo));
    scan::RunStatus run_status;
    std::optional<AppliedSettings> readback_settings;
    std::optional<std::string> readback_error;

    SampicSession* active_session = nullptr;
    const int max_session_attempts = 2;
    std::string config_error;
    for (int attempt = 0; attempt < max_session_attempts && !active_session; ++attempt) {
      try {
        auto& sess = ensure_session();
        lecroy.SetDoublePulseDelay(combo.pulse_separation_ns);
        sess.configure_for_combo(combo, config_.scan.board_index, config_.scan.channels);
        active_session = &sess;
      } catch (const std::exception& ex) {
        config_error = ex.what();
        if (attempt + 1 >= max_session_attempts) {
          combo_failed = true;
          SampleResult failure;
          failure.stats.record_error(std::string("Configuration failed: ") + config_error);
          failure.success = false;
          samples.push_back(failure);
          report_sample(samples.back(), -1, config_.timing.samples_per_combo);
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

    if (active_session && !combo_failed) {
      std::vector<std::string> start_errors;
      int start_attempts = 0;
      if (!active_session->start_run_with_retry(config_.start_retry, start_attempts, start_errors)) {
        combo_failed = true;
        run_status.run_started = false;
        run_status.start_attempts = start_attempts;
        run_status.start_errors = start_errors;
        SampleResult failure;
        failure.stats.record_error("Run failed to start after retry budget exhausted.");
        failure.success = false;
        samples.push_back(failure);
        report_sample(samples.back(), -1, config_.timing.samples_per_combo);
        session.reset();
      } else {
        run_status.run_started = true;
        run_status.start_attempts = start_attempts;
        run_status.start_errors = start_errors;
        sampic::lecroy::ManualTriggerGuard trigger_guard(manual_trigger.get());
        for (int sample_idx = 0; sample_idx < config_.timing.samples_per_combo; ++sample_idx) {
          if (stop_requested()) break;
          auto sample = collect_sample(*active_session, config_, stop_flag_);
          samples.push_back(sample);
          if (!sample.success) {
            combo_failed = true;
          }
          report_sample(samples.back(), sample_idx, config_.timing.samples_per_combo);
          if (stop_requested()) break;
        }
      }
      active_session->stop_run();
    }

    if (config_.timing.cooldown_between_combos_s > 0.0 && !stop_requested()) {
      std::this_thread::sleep_for(
          std::chrono::duration<double>(config_.timing.cooldown_between_combos_s));
    }

    const auto combo_end = std::chrono::steady_clock::now();
    const double wall_seconds = std::chrono::duration<double>(combo_end - combo_start).count();
    const std::string status = stop_requested()
                                   ? "aborted"
                                   : (combo_failed ? "failed" : "complete");
    const auto record = build_record(config_, combo, samples, readback_settings,
                                     readback_error, run_status, status, key, wall_seconds);
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
