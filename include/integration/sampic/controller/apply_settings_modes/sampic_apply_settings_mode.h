#ifndef SAMPIC_APPLY_SETTINGS_MODE_H
#define SAMPIC_APPLY_SETTINGS_MODE_H

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_crate_configurator.h"
#include "integration/sampic/config/sampic_board_configurator.h"
#include "integration/sampic/config/sampic_chip_configurator.h"
#include "integration/sampic/config/sampic_channel_configurator.h"
#include "integration/sampic/config/sampic_controller_config.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

/// Abstract base for all "apply settings" modes
class SampicApplySettingsMode {
public:
    SampicApplySettingsMode(CrateInfoStruct& info,
                            CrateParamStruct& params,
                            SampicSystemSettings& settings,
                            const SampicControllerConfig& controllerCfg)
        : info_(info),
          params_(params),
          settings_(settings),
          controllerCfg_(controllerCfg) {}

    virtual ~SampicApplySettingsMode() = default;

    /// Apply settings to hardware
    virtual void apply() = 0;

protected:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicSystemSettings& settings_;
    const SampicControllerConfig& controllerCfg_;
};

#endif // SAMPIC_APPLY_SETTINGS_MODE_H
