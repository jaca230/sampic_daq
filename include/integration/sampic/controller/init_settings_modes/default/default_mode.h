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
    SAMPIC256CH_ErrCode openCrateConnection();
    SAMPIC256CH_ErrCode checkAndLogFirmware();
    SAMPIC256CH_ErrCode resetHardware();
    SAMPIC256CH_ErrCode setDefaultParameters();
    void loadCalibration();
    void logCorrectionLevels();
    SAMPIC256CH_ErrCode allocateEventMemory();

    SampicInitSettingsModeDefaultConfig config_;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_DEFAULT_H
