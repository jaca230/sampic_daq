#ifndef SAMPIC_COLLECTOR_CONFIG_H
#define SAMPIC_COLLECTOR_CONFIG_H

#include <string>
#include <cstddef>
/// Shared collector settings. Mode-specific settings are registered beside
/// their implementations under modes/<mode-id>.
struct SampicCollectorConfig {
    std::string mode = "default";
    size_t buffer_size = 128;
    int sleep_time_us = 0;
};

#endif // SAMPIC_COLLECTOR_CONFIG_H
