#ifndef SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H
#define SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"

/// Example mode: simplified trigger-only application,
/// but still through configurators so we can modify channels etc.
class SampicApplySettingsModeExample : public SampicApplySettingsMode {
public:
    using SampicApplySettingsMode::SampicApplySettingsMode;

    void apply() override;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H
