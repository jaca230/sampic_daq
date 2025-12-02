#include "sampic_tests/lecroy/manual_trigger_controller.h"

#include <chrono>
#include <exception>
#include <iostream>

#include "sampic_tests/lecroy/lecroy_client.h"

namespace sampic::lecroy {

ManualTriggerController::ManualTriggerController(LecroyClient* client,
                                                 double interval_s,
                                                 volatile std::sig_atomic_t* stop_flag)
    : client_(client), interval_s_(interval_s), stop_flag_(stop_flag) {}

ManualTriggerController::~ManualTriggerController() {
  Stop();
}

void ManualTriggerController::Start() {
  if (!client_ || interval_s_ <= 0.0) return;
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }
  worker_ = std::thread([this]() { Run(); });
}

void ManualTriggerController::Stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) {
    return;
  }
  if (worker_.joinable()) {
    worker_.join();
  }
}

void ManualTriggerController::Run() {
  const auto sleep_duration =
      interval_s_ > 0.0 ? std::chrono::duration<double>(interval_s_) : std::chrono::duration<double>(0.0);
  while (running_.load()) {
    if (stop_flag_ && *stop_flag_) break;
    try {
      client_->Trigger();
    } catch (const std::exception& ex) {
      std::cerr << "Manual trigger error: " << ex.what() << "\n";
      break;
    }
    if (sleep_duration.count() > 0.0) {
      std::this_thread::sleep_for(sleep_duration);
    }
  }
  running_.store(false);
}

ManualTriggerGuard::ManualTriggerGuard(ManualTriggerController* controller)
    : controller_(controller) {
  if (controller_) controller_->Start();
}

ManualTriggerGuard::~ManualTriggerGuard() {
  if (controller_) controller_->Stop();
}

}  // namespace sampic::lecroy
