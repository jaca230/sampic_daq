#include "sampic_tests/modes/deadtime/mode.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

#include "sampic_tests/modes/deadtime/config.h"
#include "sampic_tests/modes/deadtime/scan_runner.h"

namespace sampic::deadtime {

DeadtimeMode::DeadtimeMode(volatile std::sig_atomic_t* stop_flag)
    : stop_flag_(stop_flag) {}

std::string DeadtimeMode::name() const {
  return "deadtime";
}

std::string DeadtimeMode::description() const {
  return "Run the pulser-driven deadtime scan harness";
}

int DeadtimeMode::run(int argc, char** argv) {
  const auto opts = parse_args(argc, argv);
  const auto cfg = load_deadtime_config(opts.config_path);
  DeadtimeScanRunner runner(cfg, stop_flag_);
  return runner.run(opts.debug_first_combo);
}

DeadtimeMode::ProgramOptions DeadtimeMode::parse_args(int argc, char** argv) {
  ProgramOptions opts;
  for (int i = 0; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + std::string(name));
      }
      return std::string(argv[++i]);
    };

    if (arg == "--config") {
      opts.config_path = require_value(arg);
    } else if (arg == "--debug-first") {
      opts.debug_first_combo = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Deadtime mode options:\n"
                << "  --config <file>   Path to the scan configuration JSON (required)\n"
                << "  --debug-first     Run only the first parameter combination with verbose logging\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown deadtime option: " + std::string(arg));
    }
  }

  if (opts.config_path.empty()) {
    throw std::runtime_error("Deadtime mode requires --config <file>");
  }

  return opts;
}

}  // namespace sampic::deadtime

