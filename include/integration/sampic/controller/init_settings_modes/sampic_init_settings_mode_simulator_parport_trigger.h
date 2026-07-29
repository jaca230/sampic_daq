#ifndef SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_PP_TRIG_H
#define SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_PP_TRIG_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"

/**
 * @brief Trigger-driven simulator initialization mode.
 *
 * This mode keeps the software-only initialization behavior while allowing
 * controller-level parport trigger infrastructure startup.
 */
class SampicInitSettingsModeSimulatorParportTrigger : public SampicInitSettingsMode {
public:
    using SampicInitSettingsMode::SampicInitSettingsMode;

    int initialize() override;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_SIMULATOR_PP_TRIG_H
