#include "integration/spdlog/logger_configurator.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

void LoggerConfigurator::configure(const LoggerConfig& cfg) {
    std::vector<spdlog::sink_ptr> sinks;

    if (cfg.to_console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    if (cfg.to_file) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.log_file, cfg.max_file_size, cfg.max_files));
    }

    if (sinks.empty()) {
        // fallback to console if nothing is selected
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>(cfg.name, sinks.begin(), sinks.end());

    // Apply pattern and level
    logger->set_pattern(cfg.log_pattern);
    auto level = spdlog::level::from_str(cfg.log_level);
    logger->set_level(level);
    logger->flush_on(spdlog::level::err);

    // Install as global default logger
    spdlog::set_default_logger(logger);
    spdlog::set_level(level); // global filter level

    spdlog::info("Logger '{}' initialized at level {}", cfg.name, cfg.log_level);
}