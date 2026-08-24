#pragma once

#include <stdexcept>
#include <string>

namespace sampic::tests {

/// A probe failure after which starting another hardware point is unsafe.
class ProbeFatalError : public std::runtime_error {
 public:
  explicit ProbeFatalError(const std::string& message)
      : std::runtime_error(message) {}
};

}  // namespace sampic::tests
