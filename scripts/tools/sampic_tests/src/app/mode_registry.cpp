#include "sampic_tests/app/mode_registry.h"

#include <stdexcept>

namespace sampic::app {

void ModeRegistry::register_mode(ModeFactory factory) {
  auto instance = factory();
  const std::string key = instance->name();
  modes_.emplace(key, std::move(instance));
}

Mode* ModeRegistry::get_mode(const std::string& name) {
  auto it = modes_.find(name);
  if (it == modes_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<std::string> ModeRegistry::mode_names() const {
  std::vector<std::string> names;
  names.reserve(modes_.size());
  for (const auto& [name, _] : modes_) {
    names.push_back(name);
  }
  return names;
}

}  // namespace sampic::app

