#ifndef SAMPIC_DAQ_SAMPIC_HARDWARE_CONTEXT_H
#define SAMPIC_DAQ_SAMPIC_HARDWARE_CONTEXT_H

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

struct SampicHardwareContext {
    CrateInfoStruct& info;
    CrateParamStruct& params;
};

#endif
