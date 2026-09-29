#include "sampic_tests/lecroy/lecroy_output_gate.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "sampic_tests/lecroy/lecroy_client.h"

namespace sampic::lecroy {

LecroyOutputGate::LecroyOutputGate(
    LecroyClient& client,
    std::vector<std::string> channels)
    : client_(client), channels_(std::move(channels)) {
  if (channels_.empty()) {
    throw std::invalid_argument(
        "LecroyOutputGate requires at least one channel");
  }
}

LecroyOutputGate::~LecroyOutputGate() {
  DisableNoThrow();
}

void LecroyOutputGate::Enable() {
  // Mark control active first so a partially completed enable operation is
  // still made safe by the destructor.
  control_active_ = true;
  for (const auto& channel : channels_) {
    client_.SetChannelDisabled(channel, false);
    if (client_.IsChannelDisabled(channel)) {
      throw std::runtime_error(
          "Lecroy " + channel + " remained disabled after enable request");
    }
  }
}

void LecroyOutputGate::Disable() {
  DisableInOrder(channels_);
}

void LecroyOutputGate::DisableInOrder(
    const std::vector<std::string>& channel_order) {
  if (channel_order.empty()) {
    throw std::invalid_argument(
        "Lecroy output disable order requires at least one channel");
  }
  for (const auto& channel : channel_order) {
    client_.SetChannelDisabled(channel, true);
    if (!client_.IsChannelDisabled(channel)) {
      throw std::runtime_error(
          "Lecroy " + channel + " remained enabled after disable request");
    }
  }
  control_active_ = false;
}

void LecroyOutputGate::DisableNoThrow() noexcept {
  if (!control_active_) {
    return;
  }

  for (const auto& channel : channels_) {
    try {
      client_.SetChannelDisabled(channel, true);
    } catch (const std::exception& error) {
      std::cerr << "Warning: failed to disable Lecroy " << channel
                << " during cleanup: " << error.what() << "\n";
    }
  }
  control_active_ = false;
}

}  // namespace sampic::lecroy
