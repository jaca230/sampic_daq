#pragma once

#include <csignal>
#include <string>

#include "sampic_tests/app/mode.h"

namespace sampic::calibration {

struct CalibrationCheckOptions {
  std::string ip = "192.168.0.4";
  int port = 27015;
  int board_index = -1;  // -1 => first detected board
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib";
  double threshold = 0.1;
  int max_events = 50;
  int prepare_interval = 100;
  int max_loops = 10000;
  int retry_sleep_us = 100;
  int samples_to_print = 16;
  bool quiet = false;
};

class CalibrationMode : public app::Mode {
 public:
  explicit CalibrationMode(volatile std::sig_atomic_t* stop_flag);

  std::string name() const override;
  std::string description() const override;
  int run(int argc, char** argv) override;

 private:
  CalibrationCheckOptions parse_args(int argc, char** argv);

  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::calibration
