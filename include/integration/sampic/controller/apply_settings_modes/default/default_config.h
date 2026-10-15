#ifndef SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_CONFIG_H
#define SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_CONFIG_H

#include <string>

struct SampicApplySettingsModeDefaultConfig {
    bool reset_on_relevant_change = true;
    bool reload_calibration = false;
    std::string calibration_directory =
        "resources/calib/Crate_PIONEER_N1_LPNHE";
};

#endif
