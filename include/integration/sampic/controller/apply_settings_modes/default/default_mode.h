#ifndef SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H
#define SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"
#include "integration/sampic/controller/apply_settings_modes/default/default_config.h"

/// Default mode: applies ALL settings using configurators
class SampicApplySettingsModeDefault : public SampicApplySettingsMode {
public:
    SampicApplySettingsModeDefault(
        SampicApplySettingsModeContext& context,
        SampicApplySettingsModeDefaultConfig config)
        : SampicApplySettingsMode(context),
          config_(std::move(config)) {}

    void apply(const ConfigStore& store) override;

private:
    void reloadCalibrationIfNeeded();
    SampicApplySettingsModeDefaultConfig config_;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H
