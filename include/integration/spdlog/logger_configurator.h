#ifndef SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIGURATOR_H
#define SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIGURATOR_H

#include "integration/spdlog/logger_config.h"

class LoggerConfigurator {
public:
    // Configure global spdlog default logger from ODB/struct settings
    static void configure(const LoggerConfig& cfg);

private:
    LoggerConfigurator() = default; // no instances
};

#endif // SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIGURATOR_H
