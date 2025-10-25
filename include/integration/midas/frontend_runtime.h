#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_RUNTIME_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_RUNTIME_H

#include <chrono>
#include <memory>
#include <string>

#include "midas.h"

#include "integration/midas/frontend_config.h"
#include "integration/spdlog/logger_config.h"

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"

#include "integration/sampic/controller/sampic_controller.h"

#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "processing/sampic_processing/collector/frontend_event_collector.h"

namespace frontend::runtime {

struct ConfigBundle {
  FrontendConfig frontend{};
  LoggerConfig logger{};
  SampicSystemSettings system{};
  SampicControllerConfig controller{};
  SampicCollectorConfig collector{};
  FrontendEventCollectorConfig frontend_collector{};
};

class Runtime {
public:
  static Runtime& instance();

  INT frontendIndex = 0;
  std::string settingsPath{};
  bool initialized = false;

  std::chrono::steady_clock::time_point lastPollTime{};
  std::chrono::microseconds pollingInterval{1'000'000};
  std::chrono::steady_clock::time_point lastEventTimestamp{
      std::chrono::steady_clock::time_point::min()};

  ConfigBundle configs{};
  std::unique_ptr<SampicController> controller;
  std::unique_ptr<FrontendEventCollector> collector;

  bool loadInitialConfigs(std::string& err);
  bool refreshConfigs(std::string& err);
  bool initializeController(std::string& err);

  void reset();

  std::string makeBankName(const std::string& prefix, int idx2d) const;
  std::string makeBankName(const std::string& prefix) const;

private:
  Runtime() = default;
};

}  // namespace frontend::runtime

#endif  // SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_RUNTIME_H

