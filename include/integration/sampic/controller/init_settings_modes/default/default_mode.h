#ifndef SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H
#define SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"
#include "integration/sampic/controller/init_settings_modes/default/default_config.h"

class SampicInitSettingsModeDefault : public SampicInitSettingsMode {
public:
    SampicInitSettingsModeDefault(
        SampicInitSettingsModeContext& context,
        SampicInitSettingsModeDefaultConfig config)
        : SampicInitSettingsMode(context),
          config_(std::move(config)) {}
    int initialize() override;
private:
    SampicInitSettingsModeDefaultConfig config_;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H
