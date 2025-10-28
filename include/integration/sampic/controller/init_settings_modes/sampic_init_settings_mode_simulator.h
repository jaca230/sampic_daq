#ifndef SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H
#define SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"

/**
 * @brief Simulator initialisation mode: bypasses hardware setup.
 */
class SampicInitSettingsModeSimulator : public SampicInitSettingsMode {
public:
    using SampicInitSettingsMode::SampicInitSettingsMode;

    int initialize() override;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H
