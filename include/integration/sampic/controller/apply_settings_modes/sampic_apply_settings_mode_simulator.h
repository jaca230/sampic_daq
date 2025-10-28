#ifndef SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H
#define SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"

/**
 * @brief Simulator apply-settings mode: no hardware interaction.
 */
class SampicApplySettingsModeSimulator : public SampicApplySettingsMode {
public:
    using SampicApplySettingsMode::SampicApplySettingsMode;

    void apply() override;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_SIMULATOR_H
