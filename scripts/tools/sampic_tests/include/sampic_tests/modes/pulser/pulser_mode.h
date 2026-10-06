#pragma once

#include <csignal>
#include <string>
#include <utility>
#include <vector>

#include "sampic_tests/app/mode.h"

namespace sampic::pulser {

struct PulserRateOptions {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib/Crate_PIONEER_N1_LPNHE";
  bool pulser_sync = true;
  bool pulser_enabled = true;
  int pulser_period_ticks = 6400;
  int pulser_width_ticks = 2;
  int frames_per_block = 31;
  double threshold = 0.1;
  int events = 500;
  double duration_s = 0.0;
  int prepare_interval = 100;
  int max_loops = 10000;
  int retry_sleep_us = 100;
  bool quiet = false;
  std::string channel_counts_csv;
  std::vector<std::pair<int, int>> enabled_channels;
  std::vector<std::pair<int, int>> disabled_channels;
};

class PulserRateMode : public app::Mode {
 public:
  explicit PulserRateMode(volatile std::sig_atomic_t* stop_flag);

  std::string name() const override;
  std::string description() const override;
  int run(int argc, char** argv) override;

 private:
  PulserRateOptions parse_args(int argc, char** argv);

  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::pulser
