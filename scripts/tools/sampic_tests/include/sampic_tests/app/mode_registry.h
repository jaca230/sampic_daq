#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "sampic_tests/app/mode.h"

namespace sampic::app {

class ModeRegistry {
 public:
  using ModeFactory = std::function<std::unique_ptr<Mode>()>;

  void register_mode(ModeFactory factory);
  Mode* get_mode(const std::string& name);
  std::vector<std::string> mode_names() const;

 private:
  std::unordered_map<std::string, std::unique_ptr<Mode>> modes_;
};

}  // namespace sampic::app

