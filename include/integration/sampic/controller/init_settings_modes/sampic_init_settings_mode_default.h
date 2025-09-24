#ifndef SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H
#define SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"

class SampicInitSettingsModeDefault : public SampicInitSettingsMode {
public:
    using SampicInitSettingsMode::SampicInitSettingsMode;
    int initialize() override;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H
