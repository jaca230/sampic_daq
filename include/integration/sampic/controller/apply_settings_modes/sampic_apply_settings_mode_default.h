#ifndef SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H
#define SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"

/// Default mode: applies ALL settings using configurators
class SampicApplySettingsModeDefault : public SampicApplySettingsMode {
public:
    using SampicApplySettingsMode::SampicApplySettingsMode;

    void apply() override;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_DEFAULT_H
