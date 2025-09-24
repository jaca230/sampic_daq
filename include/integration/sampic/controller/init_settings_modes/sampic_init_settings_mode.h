#ifndef SAMPIC_INIT_SETTINGS_MODE_H
#define SAMPIC_INIT_SETTINGS_MODE_H

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_controller_config.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicInitSettingsMode {
public:
    SampicInitSettingsMode(CrateInfoStruct& info,
                           CrateParamStruct& params,
                           void*& eventBuffer,
                           ML_Frame*& mlFrames,
                           const SampicSystemSettings& settings,
                           const SampicControllerConfig& controllerCfg)
        : info_(info),
          params_(params),
          eventBuffer_(eventBuffer),
          mlFrames_(mlFrames),
          settings_(settings),
          controllerCfg_(controllerCfg) {}

    virtual ~SampicInitSettingsMode() = default;

    virtual int initialize() = 0;

protected:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void*& eventBuffer_;
    ML_Frame*& mlFrames_;
    const SampicSystemSettings& settings_;
    const SampicControllerConfig& controllerCfg_;
};

#endif // SAMPIC_INIT_SETTINGS_MODE_H
