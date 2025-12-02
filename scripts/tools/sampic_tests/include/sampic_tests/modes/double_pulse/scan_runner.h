#pragma once

#include <csignal>
#include <unordered_set>

#include "sampic_tests/common/scan_types.h"
#include "sampic_tests/modes/double_pulse/config.h"

namespace sampic::double_pulse {

class DoublePulseScanRunner {
 public:
  DoublePulseScanRunner(DoublePulseConfig config, volatile std::sig_atomic_t* stop_flag);

  int run(bool debug_mode);

 private:
  DoublePulseConfig config_;
  volatile std::sig_atomic_t* stop_flag_ = nullptr;
};

}  // namespace sampic::double_pulse
