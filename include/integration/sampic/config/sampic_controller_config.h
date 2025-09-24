#ifndef SAMPIC_CONTROLLER_CONFIG_H
#define SAMPIC_CONTROLLER_CONFIG_H

#include <cstddef>

enum class SampicInitSettingsModeType {
    DEFAULT,
    EXAMPLE
};

enum class SampicApplySettingsModeType {
    DEFAULT,
    EXAMPLE
};

struct SampicControllerConfig {
    // Independent mode switches
    SampicInitSettingsModeType  init_mode  = SampicInitSettingsModeType::EXAMPLE;
    SampicApplySettingsModeType apply_mode = SampicApplySettingsModeType::EXAMPLE;

};

#endif // SAMPIC_CONTROLLER_CONFIG_H
