#ifndef SAMPIC_APPLY_SETTINGS_MODE_H
#define SAMPIC_APPLY_SETTINGS_MODE_H

#include <string>
#include <utility>

#include "core/config/config_store.h"
#include "integration/sampic/settings/sampic_hardware_registry.h"
#include "core/registry/mode/mode_registry.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_context.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

/// Abstract base for all "apply settings" modes
class SampicApplySettingsMode {
public:
    explicit SampicApplySettingsMode(SampicApplySettingsModeContext& context)
        : info_(context.info),
          params_(context.params),
          hardware_root_(std::move(context.hardware_root)) {}

    virtual ~SampicApplySettingsMode() = default;

    /// Apply settings to hardware
    virtual void apply(const ConfigStore& store) = 0;

protected:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    std::string hardware_root_;
};

using SampicApplySettingsModeRegistry =
    ModeRegistry<SampicApplySettingsMode, SampicApplySettingsModeContext>;

#endif // SAMPIC_APPLY_SETTINGS_MODE_H
