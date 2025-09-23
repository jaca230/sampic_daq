#ifndef SAMPIC_COLLECTOR_MODE_H
#define SAMPIC_COLLECTOR_MODE_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "integration/sampic/config/sampic_collector_config.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/// Abstract base for all SAMPIC collector modes
class SampicCollectorMode {
public:
    SampicCollectorMode(CrateInfoStruct& info,
                        CrateParamStruct& params,
                        void* eventBuffer,
                        ML_Frame* mlFrames,
                        const SampicCollectorConfig& cfg)
        : info_(info), params_(params),
          eventBuffer_(eventBuffer), mlFrames_(mlFrames),
          cfg_(cfg) {}

    virtual ~SampicCollectorMode() = default;

    /// Perform one acquisition step, filling event + timing
    virtual int readEvent(EventStruct& event,
                          SampicEventTiming& timing) = 0;

protected:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void* eventBuffer_;
    ML_Frame* mlFrames_;
    const SampicCollectorConfig& cfg_;
};

#endif // SAMPIC_COLLECTOR_MODE_H
