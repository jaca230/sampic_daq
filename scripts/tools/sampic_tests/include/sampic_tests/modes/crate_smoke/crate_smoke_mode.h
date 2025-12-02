#pragma once

#include <csignal>
#include <string>

#include "sampic_tests/app/mode.h"

namespace sampic::crate_smoke {

struct CrateSmokeOptions {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "resources/calib";
  int read_attempts = 25;
  int retry_sleep_us = 2000;
};

class CrateSmokeMode : public app::Mode {
 public:
  explicit CrateSmokeMode(volatile std::sig_atomic_t* stop_flag);

  std::string name() const override;
  std::string description() const override;
  int run(int argc, char** argv) override;

 private:
  CrateSmokeOptions parse_args(int argc, char** argv);

  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::crate_smoke
