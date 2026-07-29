#ifndef SAMPIC_COLLECTOR_MODE_H
#define SAMPIC_COLLECTOR_MODE_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "integration/sampic/collector/modes/sampic_collector_mode_context.h"
#include "core/registry/mode/mode_registry.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/**
 * @brief Abstract base for all SAMPIC collector modes.
 * Each mode defines how data is read, decoded, and pushed into the buffer.
 */
class SampicCollectorMode {
public:
    explicit SampicCollectorMode(SampicCollectorModeContext& context)
        : buffer_(context.buffer),
          info_(context.info),
          params_(context.params),
          eventBuffer_(context.event_buffer),
          mlFrames_(context.frames) {}

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
};

using SampicCollectorModeRegistry =
    ModeRegistry<SampicCollectorMode, SampicCollectorModeContext>;

#endif // SAMPIC_COLLECTOR_MODE_H
