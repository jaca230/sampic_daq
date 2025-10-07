#ifndef SAMPIC_COLLECTOR_MODE_H
#define SAMPIC_COLLECTOR_MODE_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "integration/sampic/config/sampic_collector_config.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/**
 * @brief Abstract base for all SAMPIC collector modes.
 * Each mode defines how data is read, decoded, and pushed into the buffer.
 */
class SampicCollectorMode {
public:
    SampicCollectorMode(SampicEventBuffer& buffer,
                        CrateInfoStruct& info,
                        CrateParamStruct& params,
                        void* eventBuffer,
                        ML_Frame* mlFrames,
                        const SampicCollectorConfig& cfg)
        : buffer_(buffer),
          info_(info),
          params_(params),
          eventBuffer_(eventBuffer),
          mlFrames_(mlFrames),
          cfg_(cfg) {}

    virtual ~SampicCollectorMode() = default;

    /**
     * @brief Perform one acquisition cycle.
     * The mode may push zero or more SampicEvents into the buffer.
     * @return true if successful, false on recoverable error.
     */
    virtual bool collect() = 0;

protected:
    SampicEventBuffer& buffer_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void* eventBuffer_;
    ML_Frame* mlFrames_;
    const SampicCollectorConfig& cfg_;
};

#endif // SAMPIC_COLLECTOR_MODE_H
