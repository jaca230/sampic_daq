#include "integration/midas/frontend_runtime.h"

#include <cstdio>

#include "integration/midas/frontend_odb_paths.h"
#include "integration/midas/odb/odb_manager.h"
#include "integration/spdlog/logger_configurator.h"

namespace frontend::runtime {

Runtime& Runtime::instance() {
  static Runtime instance;
  return instance;
}

bool Runtime::loadInitialConfigs(std::string& err) {
  try {
    OdbManager odb;
    const std::string& base = settingsPath;
    auto path = [&](frontend::odb::Section section) {
      return frontend::odb::make_path(base, section);
    };

    const auto logger_path = path(frontend::odb::Section::Logger);
    odb.initialize(logger_path, LoggerConfig{});
    configs.logger = odb.read<LoggerConfig>(logger_path);
    LoggerConfigurator::configure(configs.logger);

    const auto frontend_path = path(frontend::odb::Section::Frontend);
    odb.initialize(frontend_path, FrontendConfig{});
    configs.frontend = odb.read<FrontendConfig>(frontend_path);

    const auto crate_path = path(frontend::odb::Section::Crate);
    odb.initialize(crate_path, SampicSystemSettings{});
    configs.system = odb.read<SampicSystemSettings>(crate_path);

    const auto controller_path = path(frontend::odb::Section::SampicController);
    odb.initialize(controller_path, SampicControllerConfig{});
    configs.controller = odb.read<SampicControllerConfig>(controller_path);

    const auto collector_path = path(frontend::odb::Section::SampicEventCollector);
    odb.initialize(collector_path, SampicCollectorConfig{});
    configs.collector = odb.read<SampicCollectorConfig>(collector_path);

    const auto frontend_collector_path =
        path(frontend::odb::Section::FrontendEventCollector);
    odb.initialize(frontend_collector_path, FrontendEventCollectorConfig{});
    configs.frontend_collector =
        odb.read<FrontendEventCollectorConfig>(frontend_collector_path);

    pollingInterval = std::chrono::microseconds(configs.frontend.polling_interval_us);
    return true;
  } catch (const std::exception& e) {
    err = e.what();
    return false;
  }
}

bool Runtime::refreshConfigs(std::string& err) {
  try {
    OdbManager odb;
    const std::string& base = settingsPath;
    auto path = [&](frontend::odb::Section section) {
      return frontend::odb::make_path(base, section);
    };

    configs.logger = odb.read<LoggerConfig>(path(frontend::odb::Section::Logger));
    configs.frontend = odb.read<FrontendConfig>(path(frontend::odb::Section::Frontend));
    configs.system = odb.read<SampicSystemSettings>(path(frontend::odb::Section::Crate));
    configs.controller =
        odb.read<SampicControllerConfig>(path(frontend::odb::Section::SampicController));
    configs.collector =
        odb.read<SampicCollectorConfig>(path(frontend::odb::Section::SampicEventCollector));
    configs.frontend_collector =
        odb.read<FrontendEventCollectorConfig>(
            path(frontend::odb::Section::FrontendEventCollector));

    LoggerConfigurator::configure(configs.logger);
    pollingInterval = std::chrono::microseconds(configs.frontend.polling_interval_us);
    return true;
  } catch (const std::exception& e) {
    err = e.what();
    return false;
  }
}

bool Runtime::initializeController(std::string& err) {
  try {
    controller = std::make_unique<SampicController>(
        configs.system, configs.controller, configs.collector);
    const int rc = controller->initialize();
    if (rc != 0) {
      controller.reset();
      err = "SAMPIC controller initialize() failed with code " + std::to_string(rc);
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    controller.reset();
    err = e.what();
    return false;
  }
}

void Runtime::reset() {
  collector.reset();
  controller.reset();
  initialized = false;
  lastPollTime = {};
  lastEventTimestamp = std::chrono::steady_clock::time_point::min();
}

std::string Runtime::makeBankName(const std::string& prefix, int idx2d) const {
  // Optimized: avoid substr and snprintf overhead
  char buffer[5];  // "AB00\0"
  buffer[0] = prefix.empty() ? 'X' : prefix[0];
  buffer[1] = (prefix.size() < 2) ? 'X' : prefix[1];
  buffer[2] = '0' + (idx2d / 10);
  buffer[3] = '0' + (idx2d % 10);
  buffer[4] = '\0';
  return std::string(buffer, 4);  // Don't include null terminator
}

std::string Runtime::makeBankName(const std::string& prefix) const {
  return makeBankName(prefix, frontendIndex);
}

}  // namespace frontend::runtime

