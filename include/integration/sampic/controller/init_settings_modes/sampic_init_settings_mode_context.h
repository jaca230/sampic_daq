#ifndef SAMPIC_DAQ_SAMPIC_INIT_SETTINGS_MODE_CONTEXT_H
#define SAMPIC_DAQ_SAMPIC_INIT_SETTINGS_MODE_CONTEXT_H

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

struct SampicInitSettingsModeContext {
    CrateInfoStruct& info;
    CrateParamStruct& params;
    void*& event_buffer;
    ML_Frame*& frames;
};

#endif
