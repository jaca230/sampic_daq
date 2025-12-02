#include "sampic_tests/common/scan_types.h"

#include <algorithm>
#include <cmath>

namespace sampic::scan {

void SeparationAccumulator::add(double value) {
  ++count;
  sum += value;
  sum_sq += value * value;
  if (value < min) min = value;
  if (value > max) max = value;
}

void SeparationAccumulator::merge(const SeparationAccumulator& other) {
  if (other.count == 0) return;
  if (count == 0) {
    min = other.min;
    max = other.max;
  } else {
    if (other.min < min) min = other.min;
    if (other.max > max) max = other.max;
  }
  count += other.count;
  sum += other.sum;
  sum_sq += other.sum_sq;
}

nlohmann::json SeparationAccumulator::to_json() const {
  nlohmann::json j;
  j["count"] = count;
  if (count == 0) {
    j["mean_ns"] = nullptr;
    j["stddev_ns"] = nullptr;
    j["min_ns"] = nullptr;
    j["max_ns"] = nullptr;
    return j;
  }

  const double mean = sum / static_cast<double>(count);
  const double variance = std::max(0.0, (sum_sq / static_cast<double>(count)) - mean * mean);
  j["mean_ns"] = mean;
  j["stddev_ns"] = std::sqrt(variance);
  j["min_ns"] = min;
  j["max_ns"] = max;
  return j;
}

void SampleStats::record_error(const std::string& msg) {
  if (std::find(error_messages.begin(), error_messages.end(), msg) == error_messages.end()) {
    error_messages.push_back(msg);
  }
}

}  // namespace sampic::scan
