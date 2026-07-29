#ifndef SAMPIC_DAQ_SAMPIC_COLLECTOR_MODE_CONTEXT_H
#define SAMPIC_DAQ_SAMPIC_COLLECTOR_MODE_CONTEXT_H

#include "integration/sampic/collector/sampic_event_buffer.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

struct SampicCollectorModeContext {
    SampicEventBuffer& buffer;
    CrateInfoStruct& info;
    CrateParamStruct& params;
    void* event_buffer;
    ML_Frame* frames;
};

#endif
