#ifndef SAMPIC_COLLECTOR_MODE_EXAMPLE_H
#define SAMPIC_COLLECTOR_MODE_EXAMPLE_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

/**
 * @class SampicCollectorModeExample
 * @brief Example or prototype SAMPIC acquisition mode.
 *
 * This mode demonstrates the core Prepare → Read → Decode workflow
 * and is intended for testing, benchmarking, or development of
 * custom acquisition logic. It produces one SampicEvent per
 * successful read/decoding sequence.
 */
class SampicCollectorModeExample : public SampicCollectorMode {
public:
    /**
     * @brief Construct an example collector mode.
     * @param buffer Output buffer for completed SampicEvents.
     * @param info Reference to crate info structure.
     * @param params Reference to crate parameter structure.
     * @param eventBuffer Pointer to the SAMPIC hardware event buffer.
     * @param mlFrames Pointer to ML_Frame array for decoding.
     * @param cfg Global collector configuration.
     */
    SampicCollectorModeExample(SampicEventBuffer& buffer,
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
    /// Direct reference to the mode-specific configuration block.
    const SampicCollectorModeExampleConfig& mode_cfg_;
};

#endif // SAMPIC_COLLECTOR_MODE_EXAMPLE_H
