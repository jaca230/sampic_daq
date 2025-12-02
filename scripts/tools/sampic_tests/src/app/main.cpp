#include <algorithm>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sampic_tests/app/mode_registry.h"
#include "sampic_tests/modes/crate_smoke/crate_smoke_mode.h"
#include "sampic_tests/modes/deadtime/mode.h"
#include "sampic_tests/modes/double_pulse/mode.h"
#include "sampic_tests/modes/pulser/pulser_mode.h"

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void signal_handler(int) {
  g_stop_requested = 1;
}

void register_modes(sampic::app::ModeRegistry& registry) {
  registry.register_mode([&]() {
    return std::make_unique<sampic::deadtime::DeadtimeMode>(&g_stop_requested);
  });
  registry.register_mode([&]() {
    return std::make_unique<sampic::double_pulse::DoublePulseMode>(&g_stop_requested);
  });
  registry.register_mode([&]() {
    return std::make_unique<sampic::pulser::PulserRateMode>(&g_stop_requested);
  });
  registry.register_mode([&]() {
    return std::make_unique<sampic::crate_smoke::CrateSmokeMode>(&g_stop_requested);
  });
}

void print_modes(const sampic::app::ModeRegistry& registry) {
  auto names = registry.mode_names();
  std::sort(names.begin(), names.end());
  std::cout << "Available modes:\n";
  for (const auto& name : names) {
    std::cout << "  - " << name << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    sampic::app::ModeRegistry registry;
    register_modes(registry);

    std::string mode_name = "deadtime";
    std::vector<char*> forwarded_args;
    forwarded_args.reserve(static_cast<std::size_t>(argc));

    for (int i = 1; i < argc; ++i) {
      std::string_view arg{argv[i]};
      if (arg == "--mode") {
        if (i + 1 >= argc) {
          throw std::runtime_error("--mode requires a value");
        }
        mode_name = argv[++i];
      } else if (arg == "--list-modes") {
        print_modes(registry);
        return 0;
      } else if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << argv[0] << " [--mode <name>] [--list-modes] [mode args...]\n"
                  << "Available modes:\n";
        print_modes(registry);
        return 0;
      } else {
        forwarded_args.push_back(argv[i]);
      }
    }

    auto* mode = registry.get_mode(mode_name);
    if (!mode) {
      throw std::runtime_error("Unknown mode: " + mode_name);
    }
    // Mode implementations expect argc/argv referring only to mode-specific options.
    return mode->run(static_cast<int>(forwarded_args.size()),
                     forwarded_args.empty() ? nullptr : forwarded_args.data());
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}
