#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"
#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/modes/double_pulse/sampic_session.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

volatile std::sig_atomic_t stop_requested = 0;
void handle_signal(int) { stop_requested = 1; }

struct ScanConfig {
  fs::path hardware_config;
  fs::path output_root;
  std::vector<double> rates_hz;
  std::vector<int> sampling_frequencies_mhz;
  std::vector<double> separations_ns;
  std::vector<int> channels;
  int board_index = 0;
  int repetitions = 1;
  double acquisition_s = 3.0;
  double drain_s = 0.25;
  std::size_t max_captured_hits = 100000;
  std::size_t waveform_hits_per_point = 0;
  double sampic_threshold_v = 0.1;
  double lecroy_amplitude_v = 1.0;
  double lecroy_baseline_v = 0.0;
  double lecroy_width_ns = 30.0;
  double lecroy_lead_ns = 1.4;
  double lecroy_trail_ns = 1.0;
  int crate_open_max_attempts = 6;
  double crate_open_initial_delay_s = 2.0;
  double crate_open_backoff = 1.5;
};

ScanConfig load_scan_config(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("Cannot open scan config: " + path.string());
  json node; input >> node;
  ScanConfig cfg;
  cfg.hardware_config = node.at("hardware_config").get<std::string>();
  if (cfg.hardware_config.is_relative()) cfg.hardware_config = path.parent_path() / cfg.hardware_config;
  cfg.hardware_config = fs::weakly_canonical(cfg.hardware_config);
  cfg.output_root = node.at("output_root").get<std::string>();
  cfg.rates_hz = node.at("rates_hz").get<std::vector<double>>();
  cfg.sampling_frequencies_mhz =
      node.value("sampling_frequencies_mhz", std::vector<int>{});
  if (node.contains("separations_us")) {
    cfg.separations_ns = node.at("separations_us").get<std::vector<double>>();
    for (double& value : cfg.separations_ns) value *= 1000.0;
  } else {
    cfg.separations_ns = node.at("separations_ns").get<std::vector<double>>();
  }
  cfg.channels = node.value("channels", std::vector<int>{});
  cfg.board_index = node.value("board_index", 0);
  cfg.repetitions = node.value("repetitions", 1);
  cfg.acquisition_s = node.value("acquisition_s", 3.0);
  cfg.drain_s = node.value("drain_s", 0.25);
  cfg.max_captured_hits = node.value("max_captured_hits", std::size_t{100000});
  cfg.waveform_hits_per_point = node.value("waveform_hits_per_point", std::size_t{0});
  cfg.sampic_threshold_v = node.value("sampic_threshold_v", 0.1);
  if (node.contains("lecroy_channel")) {
    const auto& channel = node.at("lecroy_channel");
    cfg.lecroy_amplitude_v = channel.value("amplitude_v", 1.0);
    cfg.lecroy_baseline_v = channel.value("baseline_v", 0.0);
    cfg.lecroy_width_ns = channel.value("width_ns", 30.0);
    cfg.lecroy_lead_ns = channel.value("lead_ns", 1.4);
    cfg.lecroy_trail_ns = channel.value("trail_ns", 1.0);
  }
  if (node.contains("crate_open_retry")) {
    const auto& retry = node.at("crate_open_retry");
    cfg.crate_open_max_attempts = retry.value("max_attempts", 6);
    cfg.crate_open_initial_delay_s = retry.value("initial_delay_s", 2.0);
    cfg.crate_open_backoff = retry.value("backoff", 1.5);
  }
  if (cfg.rates_hz.empty() || cfg.sampling_frequencies_mhz.empty() ||
      cfg.separations_ns.empty() || cfg.repetitions < 1 ||
      cfg.acquisition_s <= 0 || cfg.drain_s < 0 || cfg.max_captured_hits == 0) {
    throw std::runtime_error("Invalid or empty timestamp-scan grid");
  }
  for (double value : cfg.rates_hz) if (value <= 0) throw std::runtime_error("rates_hz must be positive");
  for (int value : cfg.sampling_frequencies_mhz)
    if (value <= 0) throw std::runtime_error("sampling_frequencies_mhz must be positive");
  for (double value : cfg.separations_ns) if (value <= 0) throw std::runtime_error("separations_ns must be positive");
  if (cfg.sampic_threshold_v <= 0 || cfg.lecroy_amplitude_v <= 0 ||
      cfg.lecroy_width_ns <= 0 || cfg.lecroy_lead_ns <= 0 ||
      cfg.lecroy_trail_ns <= 0 || cfg.crate_open_max_attempts < 1 ||
      cfg.crate_open_initial_delay_s < 0 || cfg.crate_open_backoff < 1.0)
    throw std::runtime_error("Invalid stable hardware or crate retry setting");
  return cfg;
}

std::unique_ptr<sampic::double_pulse::SampicSession> open_session_with_retry(
    const ScanConfig& scan,
    const sampic::double_pulse::DoublePulseConfig& hardware) {
  double delay_s = scan.crate_open_initial_delay_s;
  for (int attempt = 1; attempt <= scan.crate_open_max_attempts; ++attempt) {
    try {
      return std::make_unique<sampic::double_pulse::SampicSession>(
          hardware.connection, hardware.external_trigger);
    } catch (const std::exception& error) {
      std::cerr << "Crate open attempt " << attempt << "/"
                << scan.crate_open_max_attempts << " failed: " << error.what() << '\n';
      if (attempt == scan.crate_open_max_attempts) throw;
      std::cerr << "Retrying in " << delay_s << " s...\n";
      std::this_thread::sleep_for(std::chrono::duration<double>(delay_s));
      delay_s *= scan.crate_open_backoff;
    }
  }
  throw std::runtime_error("Unreachable crate-open retry state");
}

std::string timestamp_name() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm tm{}; localtime_r(&value, &tm);
  std::ostringstream out; out << std::put_time(&tm, "%Y%m%d_%H%M%S");
  return out.str();
}

double median(std::vector<double> values) {
  if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
  std::sort(values.begin(), values.end());
  const auto mid = values.size() / 2;
  return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2.0;
}

struct Measurement {
  std::vector<double> short_intervals_ns;
  double median_ns = std::numeric_limits<double>::quiet_NaN();
  double mean_ns = std::numeric_limits<double>::quiet_NaN();
  double stddev_ns = std::numeric_limits<double>::quiet_NaN();
};

Measurement measure_double_pulse_intervals(
    const std::vector<sampic::scan::HitRecord>& hits,
    double rate_hz,
    int sampling_mhz) {
  std::map<std::tuple<int, int, int>, std::vector<double>> by_channel;
  for (const auto& hit : hits) {
    const double hit_timestamp_ns = hit.first_cell_ts_ns +
        hit.trigger_position_cell * (1000.0 / static_cast<double>(sampling_mhz));
    by_channel[{hit.board, hit.sampic, hit.channel}].push_back(hit_timestamp_ns);
  }
  Measurement result;
  // The inter-pair interval is milliseconds at the intended low rates, while
  // tested pulse separations are nanoseconds. This broad cut identifies the
  // short member of each alternating double-pulse pair without imposing the
  // expected timestamp scale on the result.
  const double short_interval_cut_ns = 0.25e9 / rate_hz;
  for (auto& [channel, timestamps] : by_channel) {
    std::sort(timestamps.begin(), timestamps.end());
    for (std::size_t index = 1; index < timestamps.size(); ++index) {
      const double delta = timestamps[index] - timestamps[index - 1];
      if (delta > 0 && delta < short_interval_cut_ns) result.short_intervals_ns.push_back(delta);
    }
  }
  if (!result.short_intervals_ns.empty()) {
    result.median_ns = median(result.short_intervals_ns);
    result.mean_ns = std::accumulate(result.short_intervals_ns.begin(),
                                     result.short_intervals_ns.end(), 0.0) /
                     result.short_intervals_ns.size();
    double sum_sq = 0;
    for (double value : result.short_intervals_ns) sum_sq += std::pow(value - result.mean_ns, 2);
    result.stddev_ns = std::sqrt(sum_sq / result.short_intervals_ns.size());
  }
  return result;
}

void write_hits(const fs::path& path,
                const std::vector<sampic::scan::HitRecord>& active,
                const std::vector<sampic::scan::HitRecord>& drain,
                int sampling_mhz) {
  std::ofstream out(path);
  out << "phase,hit_index,board,sampic,channel,first_cell_timestamp_ns,"
         "trigger_position_cell,hit_timestamp_ns,amplitude,baseline,tot_ns\n";
  auto write = [&](const char* phase, const auto& hits) {
    for (std::size_t index = 0; index < hits.size(); ++index) {
      const auto& hit = hits[index];
      const double timestamp_ns = hit.first_cell_ts_ns +
          hit.trigger_position_cell * (1000.0 / static_cast<double>(sampling_mhz));
      out << phase << ',' << index << ',' << hit.board << ',' << hit.sampic << ','
          << hit.channel << ',' << std::setprecision(17) << hit.first_cell_ts_ns << ','
          << hit.trigger_position_cell << ',' << timestamp_ns << ','
          << hit.amplitude << ',' << hit.baseline << ',' << hit.tot_ns << '\n';
    }
  };
  write("active", active); write("drain", drain);
}

void write_waveforms(std::ofstream& out,
                     int point_index,
                     double rate_hz,
                     int sampling_mhz,
                     double separation_ns,
                     const std::vector<sampic::scan::HitRecord>& hits,
                     std::size_t limit) {
  const std::size_t hit_limit = std::min(limit, hits.size());
  const double sample_period_ns = 1000.0 / sampling_mhz;
  for (std::size_t hit_index = 0; hit_index < hit_limit; ++hit_index) {
    const auto& hit = hits[hit_index];
    const std::size_t samples = std::min(hit.corrected_samples.size(), hit.raw_samples.size());
    for (std::size_t sample = 0; sample < samples; ++sample) {
      const double relative_time_ns =
          (static_cast<double>(sample) - hit.trigger_position_cell) * sample_period_ns;
      out << point_index << ',' << rate_hz << ',' << sampling_mhz << ','
          << separation_ns / 1000.0 << ',' << hit_index << ',' << hit.board << ','
          << hit.channel << ',' << hit.trigger_position_cell << ',' << sample << ','
          << std::setprecision(17) << relative_time_ns << ','
          << hit.corrected_samples[sample] << ',' << hit.raw_samples[sample] << ','
          << hit.adc_corrected << ',' << hit.inl_corrected << ','
          << hit.residual_pedestal_corrected << '\n';
    }
  }
}

struct Options { fs::path config; std::optional<fs::path> output; bool dry_run = false; };
Options parse_options(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--config" && index + 1 < argc) options.config = argv[++index];
    else if (arg == "--output-dir" && index + 1 < argc) options.output = fs::path(argv[++index]);
    else if (arg == "--dry-run") options.dry_run = true;
    else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: double_pulse_timestamp_scan --config FILE [--output-dir DIR] [--dry-run]\n";
      std::exit(0);
    } else throw std::runtime_error("Unknown or incomplete argument: " + arg);
  }
  if (options.config.empty()) throw std::runtime_error("--config is required");
  return options;
}

double query_number(sampic::lecroy::LecroyClient& lecroy,
                    const std::string& command) {
  const std::string response = lecroy.Query(command);
  try {
    return std::stod(response);
  } catch (const std::exception&) {
    throw std::runtime_error("Non-numeric Lecroy response to " + command +
                             ": " + response);
  }
}

void verify_lecroy_point(sampic::lecroy::LecroyClient& lecroy,
                         const std::string& channel,
                         double requested_rate_hz,
                         double requested_separation_ns) {
  const double rate_hz = query_number(lecroy, "FREQ?");
  const double delay_s = query_number(lecroy, channel + ":DEL?");
  const std::string double_pulse = lecroy.Query(channel + ":DBL?");
  const std::string disabled = lecroy.Query(channel + ":DISA?");
  const double amplitude_v = query_number(lecroy, channel + ":AMP?");
  const double baseline_v = query_number(lecroy, channel + ":BASE?");
  const double width_s = query_number(lecroy, channel + ":WID?");
  const double delay_ns = delay_s * 1e9;
  std::cout << "  Lecroy readback while inhibited: FREQ=" << rate_hz
            << " Hz, " << channel << ":DBL=" << double_pulse
            << ", DEL=" << delay_ns << " ns, WID=" << width_s * 1e9
            << " ns, AMP=" << amplitude_v << " V, BASE=" << baseline_v
            << " V, DISA=" << disabled << '\n';
  if (double_pulse != "ON" && double_pulse != "1")
    throw std::runtime_error("Lecroy double-pulse mode is not enabled");
  if (disabled != "ON" && disabled != "1")
    throw std::runtime_error("Lecroy output is not inhibited before StartRun");
  if (std::abs(rate_hz - requested_rate_hz) >
      std::max(1.0, requested_rate_hz) * 0.01)
    throw std::runtime_error("Lecroy frequency readback differs from request");
  if (std::abs(delay_ns - requested_separation_ns) >
      std::max(0.2, requested_separation_ns * 0.005))
    throw std::runtime_error("Lecroy DEL readback differs from requested separation");
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto options = parse_options(argc, argv);
    const auto scan = load_scan_config(options.config);
    auto hardware = sampic::double_pulse::load_double_pulse_config(scan.hardware_config.string());
    hardware.connection.use_external_trigger = false;
    hardware.connection.threshold_volts = scan.sampic_threshold_v;
    hardware.lecroy.channel.double_pulse_enabled = true;
    hardware.lecroy.channel.amplitude_v = scan.lecroy_amplitude_v;
    hardware.lecroy.channel.baseline_v = scan.lecroy_baseline_v;
    hardware.lecroy.channel.width_ns = scan.lecroy_width_ns;
    hardware.lecroy.channel.lead_ns = scan.lecroy_lead_ns;
    hardware.lecroy.channel.trail_ns = scan.lecroy_trail_ns;
    if (!scan.channels.empty()) hardware.scan.channels = scan.channels;
    hardware.scan.board_index = scan.board_index;
    const std::size_t points = scan.rates_hz.size() *
        scan.sampling_frequencies_mhz.size() * scan.separations_ns.size() *
        scan.repetitions;
    const fs::path output = options.output.value_or(scan.output_root / timestamp_name());
    std::cout << "Double-pulse timestamp scan: " << points << " point(s)\n"
              << "hardware config: " << scan.hardware_config << "\n"
              << "sampling frequencies: ";
    for (std::size_t index = 0; index < scan.sampling_frequencies_mhz.size(); ++index) {
      if (index) std::cout << ",";
      std::cout << scan.sampling_frequencies_mhz[index];
    }
    std::cout << " MHz\n"
              << "self-trigger channels: FEB " << hardware.scan.board_index << " [";
    for (std::size_t index = 0; index < hardware.scan.channels.size(); ++index) {
      if (index) std::cout << ",";
      std::cout << hardware.scan.channels[index];
    }
    std::cout << "]\n"
              << "SAMPIC threshold: " << scan.sampic_threshold_v << " V\n"
              << "Lecroy A stable shape: amplitude=" << scan.lecroy_amplitude_v
              << " V, baseline=" << scan.lecroy_baseline_v
              << " V, width=" << scan.lecroy_width_ns
              << " ns, lead/trail=" << scan.lecroy_lead_ns << "/"
              << scan.lecroy_trail_ns << " ns, double pulse=ON\n"
              << "output: " << output << "\n";
    if (options.dry_run) return 0;

    fs::create_directories(output);
    fs::copy_file(options.config, output / "scan_config.json", fs::copy_options::overwrite_existing);
    fs::copy_file(scan.hardware_config, output / "hardware_config.json", fs::copy_options::overwrite_existing);
    std::ofstream summary(output / "summary.csv");
    summary << "rate_hz,sampling_frequency_mhz,requested_separation_us,requested_separation_ns,repetition,active_events,active_hits,drain_hits,"
               "measured_pairs,median_separation_ns,mean_separation_ns,stddev_separation_ns,ratio_measured_to_requested\n";
    std::ofstream waveforms;
    if (scan.waveform_hits_per_point > 0) {
      waveforms.open(output / "waveforms.csv");
      waveforms << "point,rate_hz,sampling_frequency_mhz,separation_us,hit_index,feb,channel,"
                   "trigger_position_cell,sample_index,relative_time_ns,corrected_v,raw_adc,"
                   "adc_corrected,inl_corrected,residual_pedestal_corrected\n";
    }

    sampic::lecroy::LecroyClient lecroy;
    // Apply the pulse shape while keeping the generator inhibited. Each point
    // enables it only after SAMPIC256CH_StartRun succeeds.
    lecroy.Configure(hardware.lecroy, false);
    std::vector<std::string> output_channels = hardware.lecroy.channels;
    if (output_channels.empty()) output_channels.push_back(hardware.lecroy.channel.channel);
    const std::string primary_channel = output_channels.front();
    sampic::lecroy::LecroyOutputGate output_gate(lecroy, output_channels);
    output_gate.Disable();
    auto session = open_session_with_retry(scan, hardware);
    std::signal(SIGINT, handle_signal); std::signal(SIGTERM, handle_signal);

    int point_index = 0;
    for (double rate_hz : scan.rates_hz)
      for (int sampling_mhz : scan.sampling_frequencies_mhz)
        for (double separation_ns : scan.separations_ns)
      for (int repetition = 1; repetition <= scan.repetitions; ++repetition) {
        if (stop_requested) break;
        ++point_index;
        std::cout << "[" << point_index << '/' << points << "] rate=" << rate_hz
                  << " Hz, sampling=" << sampling_mhz << " MHz"
                  << ", separation=" << separation_ns/1000.0
                  << " us, repetition=" << repetition << '\n';
        output_gate.Disable();
        lecroy.SetFrequency(rate_hz);
        lecroy.SetDoublePulseDelay(separation_ns);
        verify_lecroy_point(
            lecroy, primary_channel, rate_hz, separation_ns);
        sampic::double_pulse::ParameterCombination combo;
        combo.digitizer_rate_mhz = sampling_mhz;
        combo.pulse_separation_ns = separation_ns;
        combo.threshold_volts = scan.sampic_threshold_v;
        combo.auto_conversion = true;
        session->configure_for_combo(combo, hardware.scan.board_index, hardware.scan.channels);
        int attempts = 0; std::vector<std::string> errors;
        if (!session->start_run_with_retry(hardware.start_retry, attempts, errors))
          throw std::runtime_error("SAMPIC StartRun failed at point " + std::to_string(point_index));
        bool started = true;
        try {
          output_gate.Enable();
          std::vector<sampic::scan::HitRecord> active_hits;
          const auto active_stats = session->acquire_sample(
              hardware.readout, scan.acquisition_s, &stop_requested, true,
              &active_hits, scan.max_captured_hits);
          output_gate.Disable();
          std::vector<sampic::scan::HitRecord> drain_hits;
          if (scan.drain_s > 0) session->acquire_sample(
              hardware.readout, scan.drain_s, nullptr, true, &drain_hits,
              scan.max_captured_hits);
          session->stop_run(); started = false;
          const auto measurement = measure_double_pulse_intervals(
              active_hits, rate_hz, sampling_mhz);
          std::ostringstream name;
          name << "rate_" << std::setw(6) << std::setfill('0') << std::llround(rate_hz)
               << "_sampling_" << std::setw(4) << std::setfill('0') << sampling_mhz
               << "_separation_" << std::setw(8) << std::setfill('0') << std::llround(separation_ns)
               << "_rep_" << std::setw(2) << std::setfill('0') << repetition << ".csv";
          write_hits(output / name.str(), active_hits, drain_hits, sampling_mhz);
          if (waveforms.is_open()) {
            write_waveforms(waveforms, point_index, rate_hz, sampling_mhz,
                            separation_ns, active_hits,
                            scan.waveform_hits_per_point);
            waveforms.flush();
          }
          const double ratio = measurement.median_ns / separation_ns;
          summary << std::setprecision(17) << rate_hz << ',' << sampling_mhz
                  << ',' << separation_ns/1000.0
                  << ',' << separation_ns << ',' << repetition
                  << ',' << active_stats.events << ',' << active_hits.size() << ',' << drain_hits.size()
                  << ',' << measurement.short_intervals_ns.size() << ',' << measurement.median_ns
                  << ',' << measurement.mean_ns << ',' << measurement.stddev_ns << ',' << ratio << '\n';
          summary.flush();
          if (active_stats.events == 0) {
            std::cout << "  NO SAMPIC EVENTS: no self-trigger crossed threshold on the configured channels\n";
          } else if (measurement.short_intervals_ns.empty()) {
            std::cout << "  events=" << active_stats.events << ", hits=" << active_hits.size()
                      << ", but NO DOUBLE-PULSE PAIRS were found on the same channel\n";
          } else {
            std::cout << "  events=" << active_stats.events << ", hits=" << active_hits.size()
                      << ", measured pairs=" << measurement.short_intervals_ns.size()
                      << ", median dt=" << measurement.median_ns
                      << " ns, measured/requested=" << ratio << '\n';
          }
        } catch (...) {
          output_gate.Disable();
          if (started) session->stop_run();
          throw;
        }
      }
    output_gate.Disable();
    std::cout << "Scan complete: " << output << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "double_pulse_timestamp_scan error: " << error.what() << '\n';
    return 1;
  }
}
