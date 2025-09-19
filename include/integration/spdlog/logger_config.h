#ifndef SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIG_H
#define SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIG_H

#include <string>

// Configuration for spdlog logger setup.
// Define parameters to be read from ODB or config files.
struct LoggerConfig {
    std::string name = "SAMPIC_DAQ";         // logger name (shows up in [%n])
    std::string log_level = "info";         // e.g., "trace", "debug", "info", "warn", "error"
    std::string log_pattern = "[%T] [%^%l%$] [%n] %v"; // include [%n] for logger name
    std::string log_file = "frontend.log";  // optional log file path
    bool to_console = true;                 // enable logging to console
    bool to_file = false;                   // enable logging to file
    size_t max_file_size = 5 * 1024 * 1024; // 5 MB for rotating file sink
    size_t max_files = 3;                   // number of rotated files to keep
};


#endif // SAMPIC_DAQ_INTEGRATION_SPDLOG_LOGGER_CONFIG_H
