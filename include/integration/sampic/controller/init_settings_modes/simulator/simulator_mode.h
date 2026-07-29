#ifndef SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H
#define SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"
#include "integration/sampic/controller/init_settings_modes/simulator/simulator_config.h"

/**
 * @brief Simulator initialisation mode: bypasses hardware setup.
 */
class SampicInitSettingsModeSimulator : public SampicInitSettingsMode {
public:
    SampicInitSettingsModeSimulator(
        SampicInitSettingsModeContext& context,
        SampicInitSettingsModeSimulatorConfig config)
        : SampicInitSettingsMode(context),
          config_(std::move(config)) {}
    int initialize() override;
private:
    SampicInitSettingsModeSimulatorConfig config_;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_H
