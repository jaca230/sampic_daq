#ifndef SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_CONFIG_H
#define SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_CONFIG_H

#include <string>

struct SampicApplySettingsModeDefaultConfig {
    bool reload_calibration = false;
    std::string calibration_directory = "resources/calib";
};

#endif
