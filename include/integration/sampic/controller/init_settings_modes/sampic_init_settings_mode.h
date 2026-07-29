#ifndef SAMPIC_INIT_SETTINGS_MODE_H
#define SAMPIC_INIT_SETTINGS_MODE_H

#include <utility>
#include "core/registry/mode/mode_registry.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_context.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicInitSettingsMode {
public:
    explicit SampicInitSettingsMode(SampicInitSettingsModeContext& context)
        : info_(context.info),
          params_(context.params),
          eventBuffer_(context.event_buffer),
          mlFrames_(context.frames) {}

    virtual ~SampicInitSettingsMode() = default;

    virtual int initialize() = 0;

protected:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void*& eventBuffer_;
    ML_Frame*& mlFrames_;
};

using SampicInitSettingsModeRegistry =
    ModeRegistry<SampicInitSettingsMode, SampicInitSettingsModeContext>;

#endif // SAMPIC_INIT_SETTINGS_MODE_H
