#pragma once

#include <atomic>
#include <csignal>
#include <thread>

namespace sampic::lecroy {

class LecroyClient;

class ManualTriggerController {
 public:
  ManualTriggerController(LecroyClient* client,
                          double interval_s,
                          volatile std::sig_atomic_t* stop_flag);
  ~ManualTriggerController();

  void Start();
  void Stop();

 private:
  void Run();

  LecroyClient* client_;
  double interval_s_;
  volatile std::sig_atomic_t* stop_flag_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

class ManualTriggerGuard {
 public:
  explicit ManualTriggerGuard(ManualTriggerController* controller);
  ~ManualTriggerGuard();

 private:
  ManualTriggerController* controller_;
};

}  // namespace sampic::lecroy
