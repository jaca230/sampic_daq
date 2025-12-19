#pragma once

#include <csignal>
#include <string>

#include "sampic_tests/app/mode.h"

namespace sampic::occupancy {

struct ChannelOccupancyOptions {
  std::string ip = "192.168.0.4";
  int port = 27015;
  int board_index = -1;  // -1 => scan and enable all discovered boards
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib";
  double threshold = 0.1;
  int events = 500;
  double duration_s = 0.0;
  int prepare_interval = 100;
  int max_loops = 10000;
  int retry_sleep_us = 100;
  bool quiet = false;
  bool json_output = false;
};

class ChannelOccupancyMode : public app::Mode {
 public:
  explicit ChannelOccupancyMode(volatile std::sig_atomic_t* stop_flag);

  std::string name() const override;
  std::string description() const override;
  int run(int argc, char** argv) override;

 private:
  ChannelOccupancyOptions parse_args(int argc, char** argv);

  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::occupancy
