#include "sampic_tests/modes/double_pulse/mode.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/modes/double_pulse/scan_runner.h"

namespace sampic::double_pulse {

namespace {

struct ProgramOptions {
  std::string config_path;
  bool debug_first_combo = false;
};

ProgramOptions parse_args(int argc, char** argv) {
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
      std::cout << "Double-pulse mode options:\n"
                << "  --config <file>   Path to scan configuration JSON (required)\n"
                << "  --debug-first     Run only the first combo with verbose logging\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown double-pulse option: " + std::string(arg));
    }
  }

  if (opts.config_path.empty()) {
    throw std::runtime_error("Double-pulse mode requires --config <file>");
  }
  return opts;
}

}  // namespace

DoublePulseMode::DoublePulseMode(volatile std::sig_atomic_t* stop_flag)
    : stop_flag_(stop_flag) {}

std::string DoublePulseMode::name() const {
  return "double-pulse";
}

std::string DoublePulseMode::description() const {
  return "Run the Lecroy-driven double-pulse deadtime scan";
}

int DoublePulseMode::run(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);
    auto cfg = load_double_pulse_config(opts.config_path);
    DoublePulseScanRunner runner(std::move(cfg), stop_flag_);
    return runner.run(opts.debug_first_combo);
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}

}  // namespace sampic::double_pulse
