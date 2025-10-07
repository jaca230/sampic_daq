#ifndef SAMPIC_COLLECTOR_MODE_DEFAULT_H
#define SAMPIC_COLLECTOR_MODE_DEFAULT_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

/**
 * @class SampicCollectorModeDefault
 * @brief Default acquisition mode performing the standard
 *        Prepare → Read → Decode sequence per event.
 *
 * This mode directly reads events from the SAMPIC hardware using
 * the SAMPIC256CH library calls, builds a SampicEvent for each
 * successfully decoded event, and pushes it into the buffer.
 */
class SampicCollectorModeDefault : public SampicCollectorMode {
public:
    /**
     * @brief Construct the default mode with references to system and buffer objects.
     * @param buffer Output buffer for completed SampicEvents.
     * @param info Reference to crate info structure.
     * @param params Reference to crate parameter structure.
     * @param eventBuffer Pointer to the SAMPIC hardware event buffer.
     * @param mlFrames Pointer to ML_Frame array for decoding.
     * @param cfg Global collector configuration.
     */
    SampicCollectorModeDefault(SampicEventBuffer& buffer,
                               CrateInfoStruct& info,
                               CrateParamStruct& params,
                               void* eventBuffer,
                               ML_Frame* mlFrames,
                               const SampicCollectorConfig& cfg);

    /**
     * @brief Execute one acquisition cycle (Prepare→Read→Decode).
     * @return true if successful, false on error.
     */
    bool collect() override;

private:
    /// Direct reference to the mode-specific configuration block
    const SampicCollectorModeDefaultConfig& mode_cfg_;
};

#endif // SAMPIC_COLLECTOR_MODE_DEFAULT_H
