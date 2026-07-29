#ifndef SAMPIC_DAQ_SAMPIC_APPLY_SETTINGS_MODE_CONTEXT_H
#define SAMPIC_DAQ_SAMPIC_APPLY_SETTINGS_MODE_CONTEXT_H

#include <string>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

struct SampicApplySettingsModeContext {
    CrateInfoStruct& info;
    CrateParamStruct& params;
    std::string hardware_root;
};

#endif
