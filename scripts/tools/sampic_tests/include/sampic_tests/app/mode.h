#pragma once

#include <string>

namespace sampic::app {

class Mode {
 public:
  virtual ~Mode() = default;
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual int run(int argc, char** argv) = 0;
};

}  // namespace sampic::app

