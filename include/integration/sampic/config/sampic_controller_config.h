#ifndef SAMPIC_CONTROLLER_CONFIG_H
#define SAMPIC_CONTROLLER_CONFIG_H

#include <cstddef>

enum class SampicInitSettingsMode {
    DEFAULT,
    EXAMPLE
};

enum class SampicApplySettingsMode {
    DEFAULT,
    EXAMPLE
};

struct SampicControllerConfig {
    // Independent mode switches
    SampicInitSettingsMode  init_mode  = SampicInitSettingsMode::EXAMPLE;
    SampicApplySettingsMode apply_mode = SampicApplySettingsMode::EXAMPLE;

};

#endif // SAMPIC_CONTROLLER_CONFIG_H
