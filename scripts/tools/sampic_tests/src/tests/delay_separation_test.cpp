#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sampic_tests/common/scan_types.h"
#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/manual_trigger_controller.h"
#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/modes/double_pulse/sampic_session.h"

namespace {

struct Options {
  std::string config_path;
  double short_delay_ns = -1.0;
  double long_delay_ns = -1.0;
  int samples_per_delay = -1;
};

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + std::string(name));
      }
      return std::string(argv[++i]);
    };
    if (arg == "--config") {
      opts.config_path = require_value(arg);
    } else if (arg == "--short-delay") {
      opts.short_delay_ns = std::stod(require_value(arg));
    } else if (arg == "--long-delay") {
      opts.long_delay_ns = std::stod(require_value(arg));
    } else if (arg == "--samples") {
      opts.samples_per_delay = std::stoi(require_value(arg));
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: delay_separation_test --config <file> "
                   "[--short-delay ns] [--long-delay ns] [--samples count]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  if (opts.config_path.empty()) {
    throw std::runtime_error("delay_separation_test requires --config <file>");
  }
  return opts;
}

sampic::scan::SampleStats combine(const std::vector<sampic::scan::SampleStats>& stats) {
  sampic::scan::SampleStats agg;
  for (const auto& sample : stats) {
    agg.events += sample.events;
    agg.total_hits += sample.total_hits;
    agg.total_bytes += sample.total_bytes;
    agg.retries += sample.retries;
    agg.decode_errors += sample.decode_errors;
    agg.acquisition_errors += sample.acquisition_errors;
    agg.max_loop_hits += sample.max_loop_hits;
    agg.duration_s += sample.duration_s;
    agg.hit_separation.merge(sample.hit_separation);
    agg.error_messages.insert(agg.error_messages.end(),
                              sample.error_messages.begin(),
                              sample.error_messages.end());
  }
  return agg;
}

struct DelayRunResult {
  double delay_ns = 0.0;
  std::vector<sampic::scan::SampleStats> samples;
  sampic::scan::SampleStats aggregate;
};

double hits_per_event(const sampic::scan::SampleStats& stats) {
  if (stats.events == 0) return 0.0;
  return static_cast<double>(stats.total_hits) / static_cast<double>(stats.events);
}

void print_summary(const DelayRunResult& result) {
  const auto& agg = result.aggregate;
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "\nDelay " << result.delay_ns << " ns summary:\n";
  std::cout << "  events=" << agg.events
            << " hits=" << agg.total_hits
            << " duration=" << agg.duration_s << " s\n";
  std::cout << "  hits/event=" << hits_per_event(agg)
            << " events/s=" << (agg.duration_s > 0.0 ? agg.events / agg.duration_s : 0.0)
            << " data_MB/s="
            << (agg.duration_s > 0.0
                    ? (static_cast<double>(agg.total_bytes) / (1024.0 * 1024.0)) / agg.duration_s
                    : 0.0)
            << '\n';
  if (!agg.error_messages.empty()) {
    std::cout << "  errors (" << agg.error_messages.size() << "):\n";
    for (const auto& msg : agg.error_messages) {
      std::cout << "    - " << msg << '\n';
    }
  }
}

void print_hits_for_sample(int sample_idx,
                           const std::vector<sampic::scan::HitRecord>& hits) {
  std::cout << "    Sample " << (sample_idx + 1) << " captured " << hits.size() << " hits\n";
  const std::size_t limit = 16;
  for (std::size_t i = 0; i < hits.size() && i < limit; ++i) {
    const auto& hit = hits[i];
    std::cout << "      hit[" << i << "]: board=" << hit.board
              << " sampic=" << hit.sampic
              << " channel=" << hit.channel
              << " amplitude=" << hit.amplitude
              << " baseline=" << hit.baseline
              << " tot(ns)=" << hit.tot_ns
              << " first_cell_ts(ns)=" << hit.first_cell_ts_ns << "\n";
  }
  if (hits.size() > limit) {
    std::cout << "      ... (" << (hits.size() - limit) << " more)\n";
  }
}

DelayRunResult run_delay(double delay_ns,
                         int digitizer_rate_mhz,
                         const sampic::double_pulse::DoublePulseConfig& cfg,
                         sampic::double_pulse::SampicSession& session,
                         sampic::lecroy::LecroyClient& lecroy,
                         int samples_per_delay,
                         sampic::lecroy::ManualTriggerController* manual_trigger) {
  if (cfg.scan.channels.empty()) {
    throw std::runtime_error("No channels configured for the scan.");
  }
  if (digitizer_rate_mhz <= 0) {
    throw std::runtime_error("Digitizer rate must be positive.");
  }
  lecroy.SetDoublePulseDelay(delay_ns);

  sampic::double_pulse::ParameterCombination combo;
  combo.pulse_separation_ns = delay_ns;
  combo.digitizer_rate_mhz = digitizer_rate_mhz;
  session.configure_for_combo(combo, cfg.scan.board_index, cfg.scan.channels);

  std::vector<std::string> start_errors;
  int start_attempts = 0;
  bool run_started = false;
  try {
    if (!session.start_run_with_retry(cfg.start_retry, start_attempts, start_errors)) {
      std::ostringstream oss;
      oss << "Failed to start acquisition for delay=" << delay_ns
          << "ns after " << start_attempts << " attempts.";
      if (!start_errors.empty()) {
        oss << " Errors: ";
        for (const auto& msg : start_errors) {
          oss << msg << "; ";
        }
      }
      throw std::runtime_error(oss.str());
    }
    run_started = true;

    DelayRunResult result;
    result.delay_ns = delay_ns;
    sampic::lecroy::ManualTriggerGuard trigger_guard(manual_trigger);
    for (int idx = 0; idx < samples_per_delay; ++idx) {
      std::vector<sampic::scan::HitRecord> hits;
      auto stats = session.acquire_sample(cfg.readout, cfg.timing.sample_duration_s,
                                          nullptr, true, &hits);
      result.samples.push_back(std::move(stats));
      print_hits_for_sample(idx, hits);
    }
    session.stop_run();
    result.aggregate = combine(result.samples);
    return result;
  } catch (...) {
    if (run_started) {
      session.stop_run();
    }
    throw;
  }
}

double pick_delay(double requested,
                  const std::vector<double>& available,
                  bool pick_min) {
  if (requested > 0.0) {
    return requested;
  }
  if (available.empty()) {
    throw std::runtime_error("No pulse separation values defined in configuration.");
  }
  if (pick_min) {
    return *std::min_element(available.begin(), available.end());
  }
  return *std::max_element(available.begin(), available.end());
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);
    auto cfg = sampic::double_pulse::load_double_pulse_config(opts.config_path);

    const int digitizer_rate =
        cfg.scan.digitizer_rates_mhz.empty() ? 0 : cfg.scan.digitizer_rates_mhz.front();
    const int samples_per_delay =
        opts.samples_per_delay > 0 ? opts.samples_per_delay
                                   : (cfg.timing.samples_per_combo > 0
                                          ? cfg.timing.samples_per_combo
                                          : 1);

    const double short_delay =
        pick_delay(opts.short_delay_ns, cfg.scan.pulse_separation_ns, true);
    const double long_delay =
        pick_delay(opts.long_delay_ns, cfg.scan.pulse_separation_ns, false);

    if (short_delay >= long_delay) {
      throw std::runtime_error("Short delay must be smaller than long delay.");
    }

    sampic::lecroy::LecroyClient lecroy;
    lecroy.Configure(cfg.lecroy);
    sampic::double_pulse::SampicSession session(cfg.connection, cfg.external_trigger);
    std::unique_ptr<sampic::lecroy::ManualTriggerController> manual_trigger;
    if (cfg.lecroy.manual_trigger && cfg.lecroy.manual_trigger_interval_s > 0.0) {
      manual_trigger = std::make_unique<sampic::lecroy::ManualTriggerController>(
          &lecroy, cfg.lecroy.manual_trigger_interval_s, nullptr);
    }

    std::cout << "Lecroy configuration:\n"
              << "  IP: " << cfg.lecroy.ip << ":" << cfg.lecroy.port << "\n"
              << "  Channel: " << cfg.lecroy.channel.channel << "\n"
              << "  Frequency: " << cfg.lecroy.frequency_hz << " Hz\n"
              << "  Amplitude: " << cfg.lecroy.channel.amplitude_v << " V\n"
              << "  Baseline: " << cfg.lecroy.channel.baseline_v << " V\n"
              << "  Width: " << cfg.lecroy.channel.width_ns << " ns\n"
              << "  Lead: " << cfg.lecroy.channel.lead_ns << " ns\n"
              << "  Trail: " << cfg.lecroy.channel.trail_ns << " ns\n"
              << "  Double pulse: "
              << (cfg.lecroy.channel.double_pulse_enabled ? "enabled" : "disabled") << "\n";

    try {
      sampic::double_pulse::ParameterCombination preview_combo;
      preview_combo.pulse_separation_ns = short_delay;
      preview_combo.digitizer_rate_mhz = digitizer_rate;
      session.configure_for_combo(preview_combo, cfg.scan.board_index, cfg.scan.channels);
      auto readback = session.readback_settings(cfg.scan.board_index);
      std::cout << "SAMPIC readback:\n"
                << "  Sampling frequency: " << readback.sampling_frequency_mhz << " MHz\n"
                << "  External clock: " << std::boolalpha << readback.use_external_clock << "\n"
                << "  Enabled channels (" << readback.enabled_channels.size() << "): ";
      for (std::size_t i = 0; i < readback.enabled_channels.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << readback.enabled_channels[i];
      }
      if (readback.enabled_channels.empty()) {
        std::cout << "<none>";
      }
      std::cout << "\n" << std::noboolalpha;
    } catch (const std::exception& ex) {
      std::cout << "Failed to read back SAMPIC settings: " << ex.what() << "\n";
    }

    auto short_result = run_delay(short_delay, digitizer_rate, cfg, session, lecroy,
                                  samples_per_delay, manual_trigger.get());
    auto long_result = run_delay(long_delay, digitizer_rate, cfg, session, lecroy,
                                 samples_per_delay, manual_trigger.get());

    print_summary(short_result);
    print_summary(long_result);

    const double delta_hits =
        hits_per_event(long_result.aggregate) - hits_per_event(short_result.aggregate);
    std::cout << "\nDelta hits/event (long - short): " << delta_hits << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "delay_separation_test error: " << ex.what() << "\n";
    return 1;
  }
}
