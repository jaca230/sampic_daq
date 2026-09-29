#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/modes/double_pulse/sampic_session.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void signal_handler(int) { stop_requested = 1; }

struct ProbeConfig {
  fs::path hardware_config;
  fs::path output_root;
  std::vector<double> rates_hz{100, 1000, 10000};
  std::vector<double> thresholds_v{0.05, 0.10, 0.20};
  double a_amplitude_v = 1.0;
  double phase_duration_s = 2;
  int crate_open_max_attempts = 6;
  double crate_open_initial_delay_s = 2.0;
  double crate_open_backoff = 1.5;
  int board_index = 0;
  std::vector<int> channels;
};

ProbeConfig load_config(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("Cannot open routing-probe config: " + path.string());
  json node; input >> node;
  ProbeConfig cfg;
  cfg.hardware_config = node.at("hardware_config").get<std::string>();
  if (cfg.hardware_config.is_relative()) cfg.hardware_config = path.parent_path() / cfg.hardware_config;
  cfg.hardware_config = fs::weakly_canonical(cfg.hardware_config);
  cfg.output_root = node.at("output_root").get<std::string>();
  cfg.rates_hz = node.value("rates_hz", cfg.rates_hz);
  cfg.thresholds_v = node.value("thresholds_v", cfg.thresholds_v);
  cfg.a_amplitude_v = node.value("a_amplitude_v", 1.0);
  cfg.phase_duration_s = node.value("phase_duration_s", 2.0);
  if (node.contains("crate_open_retry")) {
    const auto& retry = node.at("crate_open_retry");
    cfg.crate_open_max_attempts = retry.value("max_attempts", 6);
    cfg.crate_open_initial_delay_s = retry.value("initial_delay_s", 2.0);
    cfg.crate_open_backoff = retry.value("backoff", 1.5);
  }
  cfg.board_index = node.value("board_index", 0);
  cfg.channels = node.value("channels", std::vector<int>{});
  if (cfg.channels.empty()) for (int channel = 0; channel < 64; ++channel) cfg.channels.push_back(channel);
  if (cfg.phase_duration_s <= 0 || cfg.a_amplitude_v <= 0)
    throw std::runtime_error("Duration and amplitude must be positive");
  if (cfg.rates_hz.empty() ||
      std::any_of(cfg.rates_hz.begin(), cfg.rates_hz.end(),
                  [](double value) { return value <= 0.0; }))
    throw std::runtime_error("rates_hz must contain positive values");
  if (cfg.thresholds_v.empty() ||
      std::any_of(cfg.thresholds_v.begin(), cfg.thresholds_v.end(),
                  [](double value) { return value <= 0.0; }))
    throw std::runtime_error("thresholds_v must contain positive values");
  if (cfg.crate_open_max_attempts < 1 || cfg.crate_open_initial_delay_s < 0 ||
      cfg.crate_open_backoff < 1.0)
    throw std::runtime_error("Invalid crate_open_retry settings");
  return cfg;
}

std::unique_ptr<sampic::double_pulse::SampicSession> open_session_with_retry(
    const ProbeConfig& probe,
    const sampic::double_pulse::DoublePulseConfig& hardware,
    bool external) {
  auto connection = hardware.connection;
  connection.use_external_trigger = external;
  double delay_s = probe.crate_open_initial_delay_s;
  for (int attempt = 1; attempt <= probe.crate_open_max_attempts; ++attempt) {
    try {
      return std::make_unique<sampic::double_pulse::SampicSession>(
          connection, hardware.external_trigger);
    } catch (const std::exception& error) {
      std::cerr << "  Crate open attempt " << attempt << "/"
                << probe.crate_open_max_attempts << " failed: " << error.what() << '\n';
      if (attempt == probe.crate_open_max_attempts) throw;
      std::cerr << "  Retrying in " << delay_s << " s...\n";
      std::this_thread::sleep_for(std::chrono::duration<double>(delay_s));
      delay_s *= probe.crate_open_backoff;
    }
  }
  throw std::runtime_error("Unreachable crate-open retry state");
}

std::string timestamp_name() {
  const auto now = std::chrono::system_clock::now();
  const auto value = std::chrono::system_clock::to_time_t(now);
  std::tm tm{}; localtime_r(&value, &tm);
  char buffer[32]{}; std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);
  return buffer;
}

class SafeOutputs {
 public:
  explicit SafeOutputs(sampic::lecroy::LecroyClient& lecroy) : lecroy_(lecroy) { off(); }
  ~SafeOutputs() { try { off(); } catch (...) {} }
  void select(const std::string& output) {
    off();
    if (output == "A" || output == "B") {
      lecroy_.SetChannelDisabled(output, false);
      if (lecroy_.IsChannelDisabled(output)) throw std::runtime_error("Lecroy " + output + " failed to enable");
    } else if (output != "none") {
      throw std::runtime_error("Unknown output selection: " + output);
    }
  }
  void off() {
    for (const std::string output : {"A", "B"}) {
      lecroy_.SetChannelDisabled(output, true);
      if (!lecroy_.IsChannelDisabled(output)) throw std::runtime_error("Lecroy " + output + " failed to disable");
    }
  }
 private:
  sampic::lecroy::LecroyClient& lecroy_;
};

struct PhaseResult {
  std::string trigger_mode;
  std::string enabled_output;
  sampic::scan::SampleStats stats;
  json lecroy_readback;
  double requested_rate_hz;
  double threshold_v;
};

json read_lecroy_settings(sampic::lecroy::LecroyClient& lecroy) {
  json result;
  const std::vector<std::string> global{"*IDN?", "FREQ?", "TRMD?", "TRSL?", "TROV?"};
  const std::vector<std::string> channel{"AMP?", "BASE?", "WID?", "LEAD?", "TRAIL?",
                                         "DBL?", "DEL?", "DISA?"};
  auto query = [&](const std::string& command) {
    try { result[command] = lecroy.Query(command); }
    catch (const std::exception& error) { result[command] = std::string("<error: ") + error.what() + ">"; }
  };
  for (const auto& command : global) query(command);
  for (const std::string output : {"A", "B"})
    for (const auto& suffix : channel) query(output + ":" + suffix);
  return result;
}

void print_lecroy_settings(const json& settings) {
  auto value = [&](const std::string& key) { return settings.at(key).get<std::string>(); };
  auto scaled = [&](const std::string& key, double scale) {
    try {
      std::ostringstream text;
      text << std::setprecision(6) << std::stod(value(key)) * scale;
      return text.str();
    } catch (...) { return value(key); }
  };
  std::cout << "    Instrument: " << value("*IDN?") << '\n'
            << "    Global: frequency=" << scaled("FREQ?", 1.0)
            << " Hz, trigger mode=" << value("TRMD?")
            << ", slope=" << value("TRSL?")
            << ", trigger level=" << scaled("TROV?", 1.0) << " V\n";
  for (const std::string output : {"A", "B"}) {
    const auto key = [&](const std::string& suffix) { return output + ":" + suffix; };
    std::cout << "    Channel " << output
              << ": amplitude=" << scaled(key("AMP?"), 1.0) << " V"
              << ", baseline=" << scaled(key("BASE?"), 1.0) << " V"
              << ", width=" << scaled(key("WID?"), 1e9) << " ns"
              << ", lead=" << scaled(key("LEAD?"), 1e9) << " ns"
              << ", trail=" << scaled(key("TRAIL?"), 1e9) << " ns\n"
              << "      double pulse=" << value(key("DBL?"))
              << ", delay=" << scaled(key("DEL?"), 1e6) << " us"
              << ", disabled=" << value(key("DISA?")) << '\n';
  }
}

struct SavedPulseParameters {
  sampic::lecroy::LecroyChannelConfig parameters;
  double delay_ns = 0.0;
};

SavedPulseParameters pulse_parameters_from_readback(const json& settings,
                                                     const std::string& channel) {
  const auto number = [&](const std::string& suffix) {
    return std::stod(settings.at(channel + ":" + suffix).get<std::string>());
  };
  SavedPulseParameters result;
  result.parameters.channel = channel;
  result.parameters.amplitude_v = number("AMP?");
  result.parameters.baseline_v = number("BASE?");
  result.parameters.width_ns = number("WID?") * 1e9;
  result.parameters.lead_ns = number("LEAD?") * 1e9;
  result.parameters.trail_ns = number("TRAIL?") * 1e9;
  const auto double_pulse = settings.at(channel + ":DBL?").get<std::string>();
  result.parameters.double_pulse_enabled = double_pulse == "ON" || double_pulse == "1";
  result.delay_ns = number("DEL?") * 1e9;
  return result;
}

PhaseResult run_phase(const std::string& mode,
                      const std::string& output,
                      const ProbeConfig& probe,
                      const sampic::double_pulse::DoublePulseConfig& hardware,
                      sampic::double_pulse::SampicSession& session,
                      SafeOutputs& outputs,
                      int sampling_mhz,
                      sampic::lecroy::LecroyClient& lecroy,
                      double rate_hz,
                      double threshold_v) {
  outputs.select("none");
  lecroy.SetFrequency(rate_hz);
  sampic::double_pulse::ParameterCombination combo;
  combo.digitizer_rate_mhz = sampling_mhz;
  combo.threshold_volts = threshold_v;
  session.configure_for_combo(combo, probe.board_index, probe.channels);
  int attempts = 0; std::vector<std::string> errors;
  if (!session.start_run_with_retry(hardware.start_retry, attempts, errors))
    throw std::runtime_error("StartRun failed for " + mode + "/" + output);
  bool started = true;
  try {
    outputs.select(output);
    const auto lecroy_readback = read_lecroy_settings(lecroy);
    std::cout << "\n  Phase " << mode << "/" << output;
    std::cout << " (rate=" << rate_hz << " Hz, threshold=" << threshold_v << " V)\n";
    print_lecroy_settings(lecroy_readback);
    auto stats = session.acquire_sample(hardware.readout, probe.phase_duration_s,
                                        &stop_requested, false, nullptr);
    outputs.off();
    session.stop_run(); started = false;
    std::cout << "  " << mode << "/" << output
              << " (rate=" << rate_hz << " Hz, threshold=" << threshold_v << " V)"
              << ": events=" << stats.events
              << ", hits=" << stats.total_hits
              << ", external trigger records=" << stats.external_trigger_records << '\n';
    return {mode, output, std::move(stats), lecroy_readback, rate_hz, threshold_v};
  } catch (...) {
    outputs.off();
    if (started) session.stop_run();
    throw;
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    fs::path config_path;
    fs::path output_override;
    bool dry_run = false;
    for (int index = 1; index < argc; ++index) {
      const std::string arg = argv[index];
      if (arg == "--config" && index + 1 < argc) config_path = argv[++index];
      else if (arg == "--output-dir" && index + 1 < argc) output_override = argv[++index];
      else if (arg == "--dry-run") dry_run = true;
      else if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: lecroy_routing_probe --config FILE [--output-dir DIR] [--dry-run]\n"; return 0;
      } else throw std::runtime_error("Unknown or incomplete argument: " + arg);
    }
    if (config_path.empty()) throw std::runtime_error("--config is required");
    const auto probe = load_config(config_path);
    auto hardware = sampic::double_pulse::load_double_pulse_config(probe.hardware_config.string());
    const int sampling_mhz = hardware.scan.digitizer_rates_mhz.empty() ? 6400 : hardware.scan.digitizer_rates_mhz.front();
    const fs::path output = output_override.empty() ? probe.output_root / timestamp_name() : output_override;
    std::cout << "Lecroy routing probe: threshold/rate routing matrix\n"
              << "  rates=";
    for (double rate : probe.rates_hz) std::cout << rate << " ";
    std::cout << "Hz\n  thresholds=";
    for (double threshold : probe.thresholds_v) std::cout << threshold << " ";
    std::cout << "V\n  A amplitude=" << probe.a_amplitude_v
              << " V, duration=" << probe.phase_duration_s << " s/phase\n"
              << "  SAMPIC FEB=" << probe.board_index << ", channels=" << probe.channels.size() << "\n"
              << "  output=" << output << '\n';
    if (dry_run) return 0;

    fs::create_directories(output);
    fs::copy_file(config_path, output / "probe_config.json", fs::copy_options::overwrite_existing);
    fs::copy_file(probe.hardware_config, output / "hardware_config.json", fs::copy_options::overwrite_existing);
    sampic::lecroy::LecroyClient lecroy;
    lecroy.Connect(hardware.lecroy.ip, hardware.lecroy.port);
    SafeOutputs outputs(lecroy);
    lecroy.SetFrequency(probe.rates_hz.front());
    const auto initial_settings = read_lecroy_settings(lecroy);
    std::cout << "  Initial Lecroy state (both channels disabled)\n";
    print_lecroy_settings(initial_settings);
    const auto original_a = pulse_parameters_from_readback(initial_settings, "A");
    auto probe_a = pulse_parameters_from_readback(initial_settings, "B");
    probe_a.parameters.channel = "A";
    probe_a.parameters.amplitude_v = probe.a_amplitude_v;
    // Routing is the variable under test: make A a single pulse shaped like B.
    probe_a.parameters.double_pulse_enabled = false;
    lecroy.SetChannelPulseParameters("A", probe_a.parameters, probe_a.delay_ns);
    std::cout << "  Applied to A for this probe: B's baseline/width/lead/trail/delay, "
                 "double pulse OFF; fixed amplitude=" << probe.a_amplitude_v << " V.\n";
    std::signal(SIGINT, signal_handler); std::signal(SIGTERM, signal_handler);

    std::vector<PhaseResult> results;
    const auto run = [&](sampic::double_pulse::SampicSession& session,
                         bool external, const std::string& output,
                         double rate, double threshold) {
      const std::string mode = external ? "external" : "self";
      results.push_back(run_phase(mode, output, probe, hardware, session, outputs,
                                  sampling_mhz, lecroy, rate, threshold));
    };
    const double nominal_threshold = probe.thresholds_v[probe.thresholds_v.size() / 2];
    const double nominal_rate = probe.rates_hz.front();
    {
      std::cout << "\nOpening one persistent self-trigger crate connection.\n";
      auto session = open_session_with_retry(probe, hardware, false);
      // Self-trigger baselines isolate threshold-dependent background.
      for (double threshold : probe.thresholds_v) {
        if (stop_requested) break;
        run(*session, false, "none", nominal_rate, threshold);
      }
      // Main routing matrix: only rate and SAMPIC threshold vary.
      for (double rate : probe.rates_hz) {
        for (double threshold : probe.thresholds_v) {
          if (stop_requested) break;
          run(*session, false, "A", rate, threshold);
        }
      }
      if (!stop_requested) run(*session, false, "B", nominal_rate, nominal_threshold);
    }
    if (!stop_requested) {
      std::cout << "\nOpening one persistent external-trigger crate connection.\n";
      auto session = open_session_with_retry(probe, hardware, true);
      // Minimal routing controls at the nominal threshold. Only B's external
      // response needs a rate sweep; the negative A control runs once.
      run(*session, true, "A", nominal_rate, nominal_threshold);
      for (double rate : probe.rates_hz) {
        if (stop_requested) break;
        run(*session, true, "B", rate, nominal_threshold);
      }
      if (!stop_requested) run(*session, true, "none", nominal_rate, nominal_threshold);
    }
    outputs.off();
    lecroy.SetChannelPulseParameters("A", original_a.parameters, original_a.delay_ns);

    std::ofstream summary(output / "summary.csv");
    summary << "trigger_mode,enabled_lecroy_output,requested_rate_hz,threshold_v,duration_s,events,hits,external_trigger_records,hit_rate_hz,trigger_record_rate_hz\n";
    std::ofstream channels(output / "channel_counts.csv");
    channels << "trigger_mode,enabled_lecroy_output,requested_rate_hz,threshold_v,feb,channel,hits,hit_rate_hz\n";
    std::ofstream readbacks(output / "lecroy_readback.jsonl");
    for (const auto& result : results) {
      const auto& stats = result.stats;
      summary << result.trigger_mode << ',' << result.enabled_output << ','
              << result.requested_rate_hz << ',' << result.threshold_v << ',' << stats.duration_s
              << ',' << stats.events << ',' << stats.total_hits << ',' << stats.external_trigger_records
              << ',' << stats.total_hits/stats.duration_s << ','
              << stats.external_trigger_records/stats.duration_s << '\n';
      json readback_record{{"trigger_mode", result.trigger_mode},
                           {"enabled_lecroy_output", result.enabled_output},
                           {"requested_rate_hz", result.requested_rate_hz},
                           {"threshold_v", result.threshold_v},
                           {"settings", result.lecroy_readback}};
      readbacks << readback_record.dump() << '\n';
      for (const auto& [address, count] : stats.channel_hit_counts) {
        channels << result.trigger_mode << ',' << result.enabled_output << ','
                 << result.requested_rate_hz << ',' << result.threshold_v << ','
                 << address.first << ',' << address.second << ',' << count << ','
                 << count/stats.duration_s << '\n';
      }
    }
    std::cout << "Routing probe complete: " << output << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "lecroy_routing_probe error: " << error.what() << '\n';
    return 1;
  }
}
