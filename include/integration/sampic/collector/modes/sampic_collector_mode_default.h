#ifndef SAMPIC_COLLECTOR_MODE_DEFAULT_H
#define SAMPIC_COLLECTOR_MODE_DEFAULT_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

/// Default collector mode:
/// Simple Prepare → Read → Decode per event.
class SampicCollectorModeDefault : public SampicCollectorMode {
public:
    SampicCollectorModeDefault(CrateInfoStruct& info,
                               CrateParamStruct& params,
                               void* eventBuffer,
                               ML_Frame* mlFrames,
                               const SampicCollectorConfig& cfg)
        : SampicCollectorMode(info, params, eventBuffer, mlFrames, cfg) {}

    int readEvent(EventStruct& event,
                  SampicEventTiming& timing) override;
};

#endif // SAMPIC_COLLECTOR_MODE_DEFAULT_H
