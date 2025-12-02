#pragma once

#include <csignal>
#include <unordered_set>

#include "sampic_tests/common/scan_types.h"
#include "sampic_tests/modes/deadtime/config.h"

namespace sampic::deadtime {

class DeadtimeScanRunner {
 public:
  DeadtimeScanRunner(DeadtimeConfig config, volatile std::sig_atomic_t* stop_flag);

  int run(bool debug_mode);

 private:
  DeadtimeConfig config_;
  volatile std::sig_atomic_t* stop_flag_ = nullptr;
};

}  // namespace sampic::deadtime
