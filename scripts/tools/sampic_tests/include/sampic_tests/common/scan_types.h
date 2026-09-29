#pragma once

#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace sampic::scan {

struct SeparationAccumulator {
  std::size_t count = 0;
  double sum = 0.0;
  double sum_sq = 0.0;
  double min = std::numeric_limits<double>::infinity();
  double max = -std::numeric_limits<double>::infinity();

  void add(double value);
  void merge(const SeparationAccumulator& other);
  nlohmann::json to_json() const;
};

struct SampleStats {
  std::size_t events = 0;
  std::size_t total_hits = 0;
  std::size_t external_trigger_records = 0;
  std::size_t total_bytes = 0;
  std::size_t retries = 0;
  std::size_t decode_errors = 0;
  std::size_t acquisition_errors = 0;
  std::size_t max_loop_hits = 0;
  double duration_s = 0.0;
  SeparationAccumulator hit_separation;
  std::map<std::pair<int, int>, std::size_t> channel_hit_counts;
  std::vector<std::string> error_messages;

  void record_error(const std::string& msg);
};

struct HitRecord {
  int board = -1;
  int sampic = -1;
  int channel = -1;
  double amplitude = 0.0;
  double baseline = 0.0;
  double tot_ns = 0.0;
  double first_cell_ts_ns = 0.0;
  int trigger_position_cell = 0;
  bool adc_corrected = false;
  bool inl_corrected = false;
  bool residual_pedestal_corrected = false;
  std::vector<float> corrected_samples;
  std::vector<unsigned short> raw_samples;
};

struct SampleResult {
  SampleStats stats;
  bool success = false;
  std::vector<HitRecord> hits;
};

struct RunStatus {
  bool run_started = false;
  int start_attempts = 0;
  std::vector<std::string> start_errors;
};

}  // namespace sampic::scan
