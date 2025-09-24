#ifndef SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H
#define SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"

class SampicInitSettingsModeExample : public SampicInitSettingsMode {
public:
    using SampicInitSettingsMode::SampicInitSettingsMode;
    int initialize() override;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H
