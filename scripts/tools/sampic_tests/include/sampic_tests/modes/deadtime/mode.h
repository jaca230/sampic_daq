#pragma once

#include <csignal>
#include <string>

#include "sampic_tests/app/mode.h"

namespace sampic::deadtime {

class DeadtimeMode : public app::Mode {
 public:
  explicit DeadtimeMode(volatile std::sig_atomic_t* stop_flag);

  std::string name() const override;
  std::string description() const override;
  int run(int argc, char** argv) override;

 private:
  struct ProgramOptions {
    std::string config_path;
    bool debug_first_combo = false;
  };

  ProgramOptions parse_args(int argc, char** argv);

  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::deadtime

