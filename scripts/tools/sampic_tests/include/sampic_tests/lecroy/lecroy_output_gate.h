#pragma once

#include <string>
#include <vector>

namespace sampic::lecroy {

class LecroyClient;

/// Enables selected generator channels for a capture and guarantees that they
/// are disabled when the capture ends, including during exception unwinding.
class LecroyOutputGate {
 public:
  LecroyOutputGate(LecroyClient& client, std::vector<std::string> channels);
  ~LecroyOutputGate();

  LecroyOutputGate(const LecroyOutputGate&) = delete;
  LecroyOutputGate& operator=(const LecroyOutputGate&) = delete;

  void Enable();
  void Disable();
  void DisableInOrder(const std::vector<std::string>& channel_order);

 private:
  void DisableNoThrow() noexcept;

  LecroyClient& client_;
  std::vector<std::string> channels_;
  bool control_active_ = false;
};

}  // namespace sampic::lecroy
