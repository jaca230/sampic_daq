#pragma once

#include <cstddef>
#include <limits>
#include <string>
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
  std::size_t total_bytes = 0;
  std::size_t retries = 0;
  std::size_t decode_errors = 0;
  std::size_t acquisition_errors = 0;
  std::size_t max_loop_hits = 0;
  double duration_s = 0.0;
  SeparationAccumulator hit_separation;
  std::vector<std::string> error_messages;

  void record_error(const std::string& msg);
};

struct SampleResult {
  SampleStats stats;
  bool success = false;
};

struct RunStatus {
  bool run_started = false;
  int start_attempts = 0;
  std::vector<std::string> start_errors;
};

}  // namespace sampic::scan

