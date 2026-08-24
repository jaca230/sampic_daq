#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sampic::batching_scan {

struct BatchingScanConfig {
  std::vector<int> frames_per_block;
  std::vector<int> triggers_per_event;
  std::vector<double> lecroy_rates_hz;
  int repetitions = 1;
  double duration_s = 5.0;
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
};

struct BatchingScanPoint {
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
