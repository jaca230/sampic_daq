#pragma once

#include <csignal>
#include <string>

#include "sampic_tests/app/mode.h"

namespace sampic::double_pulse {

class DoublePulseMode : public app::Mode {
 public:
  explicit DoublePulseMode(volatile std::sig_atomic_t* stop_flag);

 std::string name() const override;
 std::string description() const override;
 int run(int argc, char** argv) override;

 private:
  volatile std::sig_atomic_t* stop_flag_;
};

}  // namespace sampic::double_pulse
