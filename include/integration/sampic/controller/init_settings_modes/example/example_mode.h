#ifndef SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H
#define SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H

#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"
#include "integration/sampic/controller/init_settings_modes/example/example_config.h"

class SampicInitSettingsModeExample : public SampicInitSettingsMode {
public:
    SampicInitSettingsModeExample(
        SampicInitSettingsModeContext& context,
        SampicInitSettingsModeExampleConfig config)
        : SampicInitSettingsMode(context),
          config_(std::move(config)) {}
    int initialize() override;
private:
    SampicInitSettingsModeExampleConfig config_;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_H
