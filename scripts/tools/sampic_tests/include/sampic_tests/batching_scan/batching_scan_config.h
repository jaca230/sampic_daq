#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "sampic_tests/batching_scan/acquisition_scheme.h"

namespace sampic::batching_scan {

struct LecroyPulseDefaults {
  bool apply = false;
  double amplitude_v = 1.0;
  double baseline_v = 0.0;
  double width_ns = 30.0;
  double lead_ns = 1.4;
  double trail_ns = 1.0;
  bool double_pulse_enabled = false;
  double delay_ns = 50.0;
};

struct BatchingScanConfig {
  std::vector<int> frames_per_block;
  std::vector<int> triggers_per_event;
  std::vector<double> lecroy_rates_hz;
  // Empty retains the historical behavior: enable every channel on every FEB.
  std::vector<int> enabled_channels;
  std::vector<AcquisitionScheme> acquisition_schemes{
      AcquisitionScheme::L2ExternalGate};
  int repetitions = 1;
  double duration_s = 5.0;
  // Let SAMPIC settle after StartRun before enabling the pulse generator.
  double post_start_settle_s = 1.0;
  std::size_t startup_waveform_hits = 0;
  int max_events = 100000;
  bool pipelined_decode = false;
  std::size_t raw_queue_capacity = 128;
  double hit_offset_ns = -470.0;
  bool auto_hit_offset = true;
  double pre_window_ns = 500.0;
  double post_window_ns = 500.0;
  int primitive_gate_clocks = 10;
  int latency_gate_clocks = 3;
  int external_gate_clocks = 5;
  double drain_quiet_ms = 100.0;
  double drain_timeout_s = 10.0;
  int max_point_retries = 5;
  double retry_delay_s = 2.0;
  std::filesystem::path output_root;
  std::filesystem::path hardware_config;
  // The common defaults preserve existing configs. Per-channel defaults, when
  // present, override the common block for that channel.
  LecroyPulseDefaults lecroy_pulse_defaults;
  LecroyPulseDefaults lecroy_a_pulse_defaults;
  LecroyPulseDefaults lecroy_b_pulse_defaults;
};

struct BatchingScanPoint {
  AcquisitionScheme acquisition_scheme = AcquisitionScheme::L2ExternalGate;
  double rate_hz = 0.0;
  int frames_per_block = 1;
  int triggers_per_event = 1;
  int repetition = 1;
};

BatchingScanConfig load_batching_scan_config(
    const std::filesystem::path& path,
    const std::filesystem::path& project_dir);

std::vector<BatchingScanPoint> build_batching_scan_points(
    const BatchingScanConfig& config);

std::string batching_scan_run_name(const BatchingScanPoint& point);

}  // namespace sampic::batching_scan
