#ifndef SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H
#define SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"
#include "integration/sampic/controller/apply_settings_modes/simulator/simulator_config.h"

/**
 * @brief Simulator apply-settings mode: no hardware interaction.
 */
class SampicApplySettingsModeSimulator : public SampicApplySettingsMode {
public:
    SampicApplySettingsModeSimulator(
        SampicApplySettingsModeContext& context,
        SampicApplySettingsModeSimulatorConfig config)
        : SampicApplySettingsMode(context),
          config_(std::move(config)) {}
    void apply(const ConfigStore&) override;
private:
    SampicApplySettingsModeSimulatorConfig config_;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H
