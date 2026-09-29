#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "acquisition_sequencer.h"
#include "runtime_and_options.h"
#include "sampic_session.h"
#include "sampic_tests/batching_scan/batching_scan_config.h"
#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"
#include "sampic_tests/modes/double_pulse/config.h"
struct BatchingScanCommand {
  std::filesystem::path config_path;
  std::optional<std::filesystem::path> output_dir;
  bool resume = false;
  bool dry_run = false;
};

struct LecroyPulseReadback {
  sampic::lecroy::LecroyChannelConfig parameters;
  double delay_ns = 0.0;
};

LecroyPulseReadback read_lecroy_pulse_settings(
    sampic::lecroy::LecroyClient& lecroy,
    const std::string& channel) {
  const auto number = [&](const std::string& suffix) {
    return std::stod(lecroy.Query(channel + ":" + suffix + "?"));
  };
  LecroyPulseReadback result;
  result.parameters.channel = channel;
  result.parameters.amplitude_v = number("AMP");
  result.parameters.baseline_v = number("BASE");
  result.parameters.width_ns = number("WID") * 1e9;
  result.parameters.lead_ns = number("LEAD") * 1e9;
  result.parameters.trail_ns = number("TRAIL") * 1e9;
  const auto double_pulse = lecroy.Query(channel + ":DBL?");
  result.parameters.double_pulse_enabled =
      double_pulse == "ON" || double_pulse == "1";
  result.delay_ns = number("DEL") * 1e9;
  return result;
}

void print_lecroy_pulse_settings(
    std::ostream& output,
    const std::string& label,
    const LecroyPulseReadback& settings) {
  output << label << ": amplitude=" << settings.parameters.amplitude_v
         << " V, baseline=" << settings.parameters.baseline_v
         << " V, width=" << settings.parameters.width_ns
         << " ns, lead=" << settings.parameters.lead_ns
         << " ns, trail=" << settings.parameters.trail_ns
         << " ns, double_pulse="
         << (settings.parameters.double_pulse_enabled ? "ON" : "OFF")
         << ", delay=" << settings.delay_ns << " ns";
}

BatchingScanCommand parse_batching_scan_command(int argc, char** argv) {
  BatchingScanCommand command;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    auto value = [&]() -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error(std::string(argument) + " requires a value");
      }
      return argv[++index];
    };
    if (argument == "--batch-scan-config") {
      command.config_path = value();
    } else if (argument == "--output-dir") {
      command.output_dir = value();
    } else if (argument == "--resume") {
      command.resume = true;
    } else if (argument == "--dry-run") {
      command.dry_run = true;
    } else {
      throw std::runtime_error(
          "Unknown batching-scan option: " + std::string(argument));
    }
  }
  if (command.config_path.empty()) {
    throw std::runtime_error("--batch-scan-config is required");
  }
  return command;
}

std::filesystem::path batching_project_dir() {
  const auto executable = std::filesystem::canonical("/proc/self/exe");
  return std::filesystem::weakly_canonical(
      executable.parent_path() / "../..");
}

std::string timestamp_directory_name() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream output;
  output << std::put_time(&local, "%Y%m%d_%H%M%S");
  return output.str();
}

std::filesystem::path latest_scan_directory(
    const std::filesystem::path& output_root) {
  std::filesystem::path latest;
  if (std::filesystem::exists(output_root)) {
    for (const auto& entry : std::filesystem::directory_iterator(output_root)) {
      if (!entry.is_directory() ||
          !std::filesystem::exists(entry.path() / "scan_manifest.csv")) {
        continue;
      }
      if (latest.empty() ||
          entry.path().filename().string() > latest.filename().string()) {
        latest = entry.path();
      }
    }
  }
  if (latest.empty()) {
    throw std::runtime_error(
        "--resume requested without --output-dir, but no prior scan exists in " +
        output_root.string());
  }
  return latest;
}

void append_manifest_row(
    const std::filesystem::path& manifest,
    const sampic::batching_scan::BatchingScanPoint& point,
    std::string_view status,
    const std::string& run_name) {
  std::ofstream output(manifest, std::ios::app);
  if (!output) {
    throw std::runtime_error("Unable to append scan manifest");
  }
  output << point.rate_hz << ',' << point.frames_per_block << ','
         << point.triggers_per_event << ','
         << sampic::batching_scan::acquisition_scheme_name(
                point.acquisition_scheme)
         << ',' << point.repetition << ','
         << status << ',' << run_name << '\n';
}

int run_persistent_batching_scan(int argc, char** argv) {
  const auto command = parse_batching_scan_command(argc, argv);
  const auto project_dir = batching_project_dir();
  const auto scan_config = sampic::batching_scan::load_batching_scan_config(
      command.config_path, project_dir);
  const auto points =
      sampic::batching_scan::build_batching_scan_points(scan_config);
  const auto output_dir = command.output_dir
                              ? std::filesystem::absolute(*command.output_dir)
                          : command.resume
                              ? latest_scan_directory(scan_config.output_root)
                              : scan_config.output_root /
                                    timestamp_directory_name();

  std::cout << "Persistent batching scan: " << points.size() << " point(s)\n"
            << "  scan config: " << std::filesystem::absolute(command.config_path)
            << "\n  hardware config: " << scan_config.hardware_config
            << "\n  output: " << output_dir << "\n"
            << "  enabled channels: ";
  if (scan_config.enabled_channels.empty()) {
    std::cout << "all channels on all FEBs";
  } else {
    std::cout << "FEB selected by hardware config [";
    for (std::size_t index = 0; index < scan_config.enabled_channels.size(); ++index) {
      if (index) std::cout << ',';
      std::cout << scan_config.enabled_channels[index];
    }
    std::cout << ']';
  }
  std::cout << "\n"
            << "  crate policy: one persistent connection; reconnect only "
               "after point failure\n";
  if (command.dry_run) return 0;

  std::filesystem::create_directories(output_dir);
  const auto manifest = output_dir / "scan_manifest.csv";
  if (!std::filesystem::exists(manifest)) {
    std::ofstream output(manifest);
    output << "requested_rate_hz,frames_per_block,triggers_per_event,"
              "acquisition_scheme,repetition,status,run_dir\n";
  }
  {
    std::ofstream output(output_dir / "scan_config.env");
    output << "source_config=" << std::filesystem::absolute(command.config_path)
           << '\n';
    output << "persistent_crate_connection=true\n";
    output << "frames=";
    for (std::size_t index = 0; index < scan_config.frames_per_block.size(); ++index) {
      if (index) output << ',';
      output << scan_config.frames_per_block[index];
    }
    output << "\ntriggers=";
    for (std::size_t index = 0; index < scan_config.triggers_per_event.size(); ++index) {
      if (index) output << ',';
      output << scan_config.triggers_per_event[index];
    }
    output << '\n';
    output << "enabled_channels=";
    if (scan_config.enabled_channels.empty()) {
      output << "all";
    } else {
      for (std::size_t index = 0; index < scan_config.enabled_channels.size(); ++index) {
        if (index) output << ',';
        output << scan_config.enabled_channels[index];
      }
    }
    output << '\n';
    output << "acquisition_schemes=";
    for (std::size_t index = 0;
         index < scan_config.acquisition_schemes.size();
         ++index) {
      if (index) output << ',';
      output << sampic::batching_scan::acquisition_scheme_name(
          scan_config.acquisition_schemes[index]);
    }
    output << '\n';
    output << "rates_hz=";
    for (std::size_t index = 0; index < scan_config.lecroy_rates_hz.size(); ++index) {
      if (index) output << ',';
      output << scan_config.lecroy_rates_hz[index];
    }
    output << "\nduration_s=" << scan_config.duration_s
           << "\npost_start_settle_s=" << scan_config.post_start_settle_s
           << "\nstartup_waveform_hits=" << scan_config.startup_waveform_hits
           << "\nmax_events=" << scan_config.max_events
           << "\npipelined_decode="
           << (scan_config.pipelined_decode ? "true" : "false")
           << "\nraw_queue_capacity=" << scan_config.raw_queue_capacity
           << "\nrepetitions=" << scan_config.repetitions
           << "\nmax_point_retries=" << scan_config.max_point_retries
           << "\nretry_delay_s=" << scan_config.retry_delay_s
           << "\napply_lecroy_channel_defaults="
           << (scan_config.lecroy_pulse_defaults.apply ? "true" : "false")
           << "\nlecroy_amplitude_v="
           << scan_config.lecroy_pulse_defaults.amplitude_v
           << "\nlecroy_baseline_v="
           << scan_config.lecroy_pulse_defaults.baseline_v
           << "\nlecroy_width_ns="
           << scan_config.lecroy_pulse_defaults.width_ns
           << "\nlecroy_lead_ns="
           << scan_config.lecroy_pulse_defaults.lead_ns
           << "\nlecroy_trail_ns="
           << scan_config.lecroy_pulse_defaults.trail_ns
           << "\nlecroy_double_pulse="
           << (scan_config.lecroy_pulse_defaults.double_pulse_enabled
                   ? "true" : "false")
           << "\nlecroy_delay_ns="
           << scan_config.lecroy_pulse_defaults.delay_ns
           << "\nlecroy_a_defaults_override="
           << (scan_config.lecroy_a_pulse_defaults.apply ? "true" : "false")
           << "\nlecroy_a_amplitude_v="
           << scan_config.lecroy_a_pulse_defaults.amplitude_v
           << "\nlecroy_a_width_ns="
           << scan_config.lecroy_a_pulse_defaults.width_ns
           << "\nlecroy_b_defaults_override="
           << (scan_config.lecroy_b_pulse_defaults.apply ? "true" : "false")
           << "\nlecroy_b_amplitude_v="
           << scan_config.lecroy_b_pulse_defaults.amplitude_v
           << "\nlecroy_b_width_ns="
           << scan_config.lecroy_b_pulse_defaults.width_ns
           << "\nmtu_bytes=1500\nsingle_frame_bytes=160\n"
              "largest_unfragmented_frames_per_block=8\n";
  }

  const auto hardware_config =
      sampic::double_pulse::load_double_pulse_config(
          scan_config.hardware_config.string());
  sampic::lecroy::LecroyClient lecroy;
  lecroy.Connect(hardware_config.lecroy.ip, hardware_config.lecroy.port);
  // Channel A carries the analog test pulse and channel B carries the
  // external gate. Both must be inhibited before startup and cutoff; disabling
  // only B leaves plain self-trigger acquisition active throughout the drain.
  sampic::lecroy::LecroyOutputGate lecroy_output(lecroy, {"A", "B"});
  try {
    lecroy_output.Disable();
  } catch (const std::exception& error) {
    throw sampic::tests::ProbeFatalError(
        "Unable to establish safe Lecroy output state: " +
        std::string(error.what()));
  }

  const auto& defaults_a = scan_config.lecroy_a_pulse_defaults.apply
      ? scan_config.lecroy_a_pulse_defaults
      : scan_config.lecroy_pulse_defaults;
  const auto& defaults_b = scan_config.lecroy_b_pulse_defaults.apply
      ? scan_config.lecroy_b_pulse_defaults
      : scan_config.lecroy_pulse_defaults;
  if (defaults_a.apply || defaults_b.apply) {
    const auto apply_defaults = [&](const std::string& channel,
                                    const auto& defaults) {
      if (!defaults.apply) return;
      sampic::lecroy::LecroyChannelConfig requested;
      requested.channel = channel;
      requested.amplitude_v = defaults.amplitude_v;
      requested.baseline_v = defaults.baseline_v;
      requested.width_ns = defaults.width_ns;
      requested.lead_ns = defaults.lead_ns;
      requested.trail_ns = defaults.trail_ns;
      requested.double_pulse_enabled = defaults.double_pulse_enabled;
      lecroy.SetChannelPulseParameters(channel, requested, defaults.delay_ns);
    };
    apply_defaults("A", defaults_a);
    apply_defaults("B", defaults_b);
    const auto applied_a = read_lecroy_pulse_settings(lecroy, "A");
    const auto applied_b = read_lecroy_pulse_settings(lecroy, "B");
    const auto close = [](double left, double right) {
      return std::abs(left - right) <=
          std::max(1e-9, std::max(std::abs(left), std::abs(right)) * 1e-4);
    };
    const auto matches_defaults = [&](const LecroyPulseReadback& applied,
                                      const auto& defaults) {
      if (!defaults.apply) return true;
      return close(applied.parameters.amplitude_v, defaults.amplitude_v) &&
          close(applied.parameters.baseline_v, defaults.baseline_v) &&
          close(applied.parameters.width_ns, defaults.width_ns) &&
          close(applied.parameters.lead_ns, defaults.lead_ns) &&
          close(applied.parameters.trail_ns, defaults.trail_ns) &&
          close(applied.delay_ns, defaults.delay_ns) &&
          applied.parameters.double_pulse_enabled ==
              defaults.double_pulse_enabled;
    };
    if (!matches_defaults(applied_a, defaults_a) ||
        !matches_defaults(applied_b, defaults_b)) {
      throw sampic::tests::ProbeFatalError(
          "Lecroy A/B readback does not match configured golden defaults");
    }
    std::cout << "Lecroy golden pulse configuration verified.\n  ";
    print_lecroy_pulse_settings(std::cout, "A applied", applied_a);
    std::cout << "\n  ";
    print_lecroy_pulse_settings(std::cout, "B applied", applied_b);
    std::cout << '\n';
    std::ofstream settings_log(output_dir / "lecroy_settings.txt");
    print_lecroy_pulse_settings(settings_log, "A applied", applied_a);
    settings_log << '\n';
    print_lecroy_pulse_settings(settings_log, "B applied", applied_b);
    settings_log << '\n';
  }

  const int sampling_rate_mhz =
      hardware_config.scan.digitizer_rates_mhz.empty()
          ? 6400
          : hardware_config.scan.digitizer_rates_mhz.front();
  std::unique_ptr<SimpleSession> session;
  bool session_needs_static_settings = false;
  std::optional<sampic::batching_scan::AcquisitionScheme>
      configured_acquisition_scheme;
  auto ensure_session = [&]() -> SimpleSession& {
    if (!session) {
      session = std::make_unique<SimpleSession>(
          hardware_config.connection,
          hardware_config.external_trigger,
          true);
      session->set_sampling_rate(sampling_rate_mhz);
      session_needs_static_settings = true;
      configured_acquisition_scheme.reset();
      std::cout << "Persistent crate connection initialized." << std::endl;
    }
    return *session;
  };

  std::signal(SIGINT, batching_scan_signal_handler);
  std::signal(SIGTERM, batching_scan_signal_handler);
  std::size_t failed_points = 0;
  const auto scan_start = std::chrono::steady_clock::now();

  for (std::size_t point_index = 0; point_index < points.size(); ++point_index) {
    const auto& point = points[point_index];
    const auto run_name = sampic::batching_scan::batching_scan_run_name(point);
    const auto run_dir = output_dir / run_name;
    if (command.resume &&
        std::filesystem::exists(run_dir / "metadata.json")) {
      std::cout << '[' << point_index + 1 << '/' << points.size()
                << "] Reusing " << run_name << '\n';
      append_manifest_row(manifest, point, "reused", run_name);
      continue;
    }
    if (g_stop_after_current_point) break;

    std::filesystem::create_directories(run_dir);
    std::cout << "\n[" << point_index + 1 << '/' << points.size() << "] "
              << run_name << '\n';
    bool complete = false;
    int attempts_used = 0;
    std::string last_error;
    for (int attempt = 1;
         attempt <= scan_config.max_point_retries + 1;
         ++attempt) {
      attempts_used = attempt;
      try {
        std::cout << "Preparing point options." << std::endl;
        Options options;
        options.config_path = scan_config.hardware_config.string();
        options.output_dir = run_dir.string();
        options.max_events = scan_config.max_events;
        options.max_duration_s = scan_config.duration_s;
        options.post_start_settle_s = scan_config.post_start_settle_s;
        options.startup_waveform_hits = scan_config.startup_waveform_hits;
        options.pipelined_decode = scan_config.pipelined_decode;
        options.raw_queue_capacity = scan_config.raw_queue_capacity;
        options.use_self_trigger_channels =
            point.acquisition_scheme !=
            sampic::batching_scan::AcquisitionScheme::External;
        options.use_l2_external_gate =
            point.acquisition_scheme ==
            sampic::batching_scan::AcquisitionScheme::L2ExternalGate;
        options.acquisition_scheme =
            sampic::batching_scan::acquisition_scheme_name(
                point.acquisition_scheme);
        options.skip_lecroy = true;
        options.manage_lecroy_output = true;
        options.lecroy_output_channel = "A,B";
        options.lecroy_rate_hz = point.rate_hz;
        options.summary_only = true;
        options.all_channels = scan_config.enabled_channels.empty();
        options.persistent_session = true;
        options.drain_quiet_ms = scan_config.drain_quiet_ms;
        options.drain_timeout_s = scan_config.drain_timeout_s;
        options.primitive_gate_clocks = scan_config.primitive_gate_clocks;
        options.latency_gate_clocks = scan_config.latency_gate_clocks;
        options.external_gate_clocks = scan_config.external_gate_clocks;
        options.frames_per_block = point.frames_per_block;
        options.triggers_per_event = point.triggers_per_event;
        options.hit_time_offset_ns = scan_config.hit_offset_ns;
        options.requested_hit_time_offset_ns = scan_config.hit_offset_ns;
        options.auto_hit_offset = scan_config.auto_hit_offset;
        options.pre_window_ns = scan_config.pre_window_ns;
        options.post_window_ns = scan_config.post_window_ns;
        std::cout << "Point options prepared." << std::endl;
        auto& active_session = ensure_session();
        options.sampling_frequency_requested_mhz =
            active_session.sampling_frequency_requested_mhz();
        options.sampling_frequency_readback_mhz =
            active_session.sampling_frequency_readback_mhz();
        options.sampling_frequency_external_clock =
            active_session.sampling_frequency_external_clock();
        // Match the known-good single-probe initialization order on every
        // fresh connection: sampling rate, packetization, then channels/L2.
        // StopRun() resets frames-per-block, so packetization is reapplied for
        // every point even while the crate connection remains open.
        std::cout << "Applying transport packetization: frames_per_block="
                  << point.frames_per_block
                  << ", triggers_per_event=" << point.triggers_per_event
                  << std::endl;
        active_session.set_packetization(
            point.frames_per_block, point.triggers_per_event);
        if (session_needs_static_settings ||
            configured_acquisition_scheme != point.acquisition_scheme) {
          std::cout << "Applying acquisition scheme: "
                    << sampic::batching_scan::acquisition_scheme_name(
                           point.acquisition_scheme)
                    << std::endl;
          AcquisitionSequencer::ConfigureSession(
              active_session,
              hardware_config,
              scan_config,
              point.acquisition_scheme);
          session_needs_static_settings = false;
          configured_acquisition_scheme = point.acquisition_scheme;
          std::cout << "Persistent crate session initialized." << std::endl;
        }
        // Some vendor routines install process signal handlers. Restore our
        // diagnostic handlers after hardware configuration so a native fault
        // produces a usable backtrace in the screen log.
        install_crash_signal_handlers();
        std::cout << "Calling capture_batching_point." << std::endl;
        AcquisitionSequencer(
            hardware_config, active_session, lecroy, lecroy_output)
            .Capture(options);
        complete = true;
        break;
      } catch (const sampic::tests::ProbeFatalError&) {
        append_manifest_row(manifest, point, "fatal", run_name);
        throw;
      } catch (const std::exception& error) {
        last_error = error.what();
        std::cerr << "Point attempt " << attempt << " failed: "
                  << last_error
                  << "\nRefreshing instrument connections before retry.\n";
        session.reset();
        session_needs_static_settings = false;
        configured_acquisition_scheme.reset();
        if (attempt <= scan_config.max_point_retries) {
          try {
            lecroy.Reconnect();
            lecroy_output.Disable();
          } catch (const std::exception& reconnect_error) {
            throw sampic::tests::ProbeFatalError(
                "Unable to restore safe Lecroy control for retry: " +
                std::string(reconnect_error.what()));
          }
          std::this_thread::sleep_for(
              std::chrono::duration<double>(scan_config.retry_delay_s));
        }
      }
    }

    {
      std::ofstream log(run_dir / "run.log", std::ios::app);
      log << (complete ? "complete" : "failed") << " after "
          << attempts_used << " attempt(s)";
      if (!last_error.empty()) log << ": " << last_error;
      log << '\n';
    }
    if (complete) {
      append_manifest_row(manifest, point, "complete", run_name);
    } else {
      ++failed_points;
      append_manifest_row(manifest, point, "failed", run_name);
    }

    const double elapsed = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - scan_start)
                               .count();
    const double average = elapsed / static_cast<double>(point_index + 1);
    const double remaining = average *
        static_cast<double>(points.size() - point_index - 1);
    {
      const auto previous_flags = std::cout.flags();
      const auto previous_precision = std::cout.precision();
      std::cout << "Progress: " << point_index + 1 << '/' << points.size()
                << "; average " << std::fixed << std::setprecision(1)
                << average << " s/point; ETA " << remaining / 3600.0
                << " h\n";
      std::cout.flags(previous_flags);
      std::cout.precision(previous_precision);
    }
    if (g_stop_after_current_point) break;
  }

  lecroy_output.Disable();
  session.reset();
  std::cout << "Batching scan finished safely. Non-fatal failed points: "
            << failed_points << "\n";
  return failed_points == 0 ? 0 : 1;
}
