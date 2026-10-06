#ifndef SAMPIC_DAQ_FRONTEND_CONFIG_BUNDLE_H
#define SAMPIC_DAQ_FRONTEND_CONFIG_BUNDLE_H

#include "integration/midas/frontend_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/spdlog/logger_config.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"

struct FrontendConfigBundle {
    FrontendConfig frontend{};
    LoggerConfig logger{};
    SampicControllerConfig controller{};
    SampicCollectorConfig collector{};
    FrontendEventCollectorConfig frontend_collector{};
};

#endif
