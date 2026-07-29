#ifndef SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H
#define SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H

#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"
#include "integration/sampic/controller/apply_settings_modes/example/example_config.h"

/// Example mode: simplified trigger-only application,
/// but still through configurators so we can modify channels etc.
class SampicApplySettingsModeExample : public SampicApplySettingsMode {
public:
    SampicApplySettingsModeExample(
        SampicApplySettingsModeContext& context,
        SampicApplySettingsModeExampleConfig config)
        : SampicApplySettingsMode(context),
          config_(std::move(config)) {}
    void apply(const ConfigStore& store) override;
private:
    SampicApplySettingsModeExampleConfig config_;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_EXAMPLE_H
