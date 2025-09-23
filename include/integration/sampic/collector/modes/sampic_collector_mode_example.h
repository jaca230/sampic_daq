#ifndef SAMPIC_COLLECTOR_MODE_EXAMPLE_H
#define SAMPIC_COLLECTOR_MODE_EXAMPLE_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

/// Example mode: placeholder for testing / prototyping custom logic.
class SampicCollectorModeExample : public SampicCollectorMode {
public:
    SampicCollectorModeExample(CrateInfoStruct& info,
                               CrateParamStruct& params,
                               void* eventBuffer,
                               ML_Frame* mlFrames,
                               const SampicCollectorConfig& cfg)
        : SampicCollectorMode(info, params, eventBuffer, mlFrames, cfg) {}

    int readEvent(EventStruct& event,
                  SampicEventTiming& timing) override;
};

#endif // SAMPIC_COLLECTOR_MODE_EXAMPLE_H
