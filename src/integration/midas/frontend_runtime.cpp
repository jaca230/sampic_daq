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

    const auto controller_path = path(frontend::odb::Section::SampicController);
    odb.initializeValue(controller_path + "/init_mode", std::string("default"));
    odb.initializeValue(controller_path + "/apply_mode", std::string("default"));
    configs.controller.init_mode = odb.readValue<std::string>(controller_path + "/init_mode");
    configs.controller.apply_mode = odb.readValue<std::string>(controller_path + "/apply_mode");
    SampicController::initializeOdb(
        odb, controller_path + "/init_modes", controller_path + "/apply_modes");

    const auto collector_path = path(frontend::odb::Section::SampicEventCollector);
    odb.initializeValue(collector_path + "/mode", std::string("default"));
    odb.initializeValue(collector_path + "/buffer_size", std::size_t(128));
    odb.initializeValue(collector_path + "/sleep_time_us", 0);
    configs.collector.mode = odb.readValue<std::string>(collector_path + "/mode");
    configs.collector.buffer_size = odb.readValue<std::size_t>(collector_path + "/buffer_size");
    configs.collector.sleep_time_us = odb.readValue<int>(collector_path + "/sleep_time_us");
    SampicCollector::initializeOdb(odb, collector_path + "/modes");

    const auto frontend_collector_path =
        path(frontend::odb::Section::FrontendEventCollector);
    odb.initializeValue(frontend_collector_path + "/mode", std::string("default"));
    odb.initializeValue(frontend_collector_path + "/buffer_size", std::uint32_t(512));
    odb.initializeValue(frontend_collector_path + "/sleep_time_us", std::uint32_t(1000));
    odb.initialize(frontend_collector_path + "/diagnostics",
                   FrontendCollectorDiagnosticsConfig{});
    configs.frontend_collector.mode =
        odb.readValue<std::string>(frontend_collector_path + "/mode");
    configs.frontend_collector.buffer_size =
        odb.readValue<std::uint32_t>(frontend_collector_path + "/buffer_size");
    configs.frontend_collector.sleep_time_us =
        odb.readValue<std::uint32_t>(frontend_collector_path + "/sleep_time_us");
    configs.frontend_collector.diagnostics =
        odb.read<FrontendCollectorDiagnosticsConfig>(
            frontend_collector_path + "/diagnostics");
    FrontendEventCollector::initializeOdb(odb, frontend_collector_path + "/modes");

    const auto hardware_path = path(frontend::odb::Section::Crate);
    SampicHardwareRegistry::catalog().initializeOdb(odb, hardware_path);

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
    const auto controller_path = path(frontend::odb::Section::SampicController);
    configs.controller.init_mode = odb.readValue<std::string>(controller_path + "/init_mode");
    configs.controller.apply_mode = odb.readValue<std::string>(controller_path + "/apply_mode");
    const auto collector_path = path(frontend::odb::Section::SampicEventCollector);
    configs.collector.mode = odb.readValue<std::string>(collector_path + "/mode");
    configs.collector.buffer_size = odb.readValue<std::size_t>(collector_path + "/buffer_size");
    configs.collector.sleep_time_us = odb.readValue<int>(collector_path + "/sleep_time_us");
    const auto frontend_collector_path =
        path(frontend::odb::Section::FrontendEventCollector);
    configs.frontend_collector.mode =
        odb.readValue<std::string>(frontend_collector_path + "/mode");
    configs.frontend_collector.buffer_size =
        odb.readValue<std::uint32_t>(frontend_collector_path + "/buffer_size");
    configs.frontend_collector.sleep_time_us =
        odb.readValue<std::uint32_t>(frontend_collector_path + "/sleep_time_us");
    configs.frontend_collector.diagnostics =
        odb.read<FrontendCollectorDiagnosticsConfig>(
            frontend_collector_path + "/diagnostics");

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
    OdbManager odb;
    const auto controller_path =
        frontend::odb::make_path(settingsPath, frontend::odb::Section::SampicController);
    const auto collector_path =
        frontend::odb::make_path(settingsPath, frontend::odb::Section::SampicEventCollector);
    const auto hardware_path =
        frontend::odb::make_path(settingsPath, frontend::odb::Section::Crate);
    controller = std::make_unique<SampicController>(
        configs.controller, configs.collector, odb,
        controller_path + "/init_modes", controller_path + "/apply_modes",
        collector_path + "/modes", hardware_path);
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
