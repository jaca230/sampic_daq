#ifndef SAMPIC_COLLECTOR_MODE_SIMULATOR_H
#define SAMPIC_COLLECTOR_MODE_SIMULATOR_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

#include <random>

/**
 * @brief Software-only SAMPIC collector used for throughput testing.
 *
 * The simulator bypasses the vendor library and produces synthetic
 * EventStruct payloads that mirror the structure expected by the rest of the
 * DAQ pipeline.
 */
class SampicCollectorModeSimulator : public SampicCollectorMode {
public:
    SampicCollectorModeSimulator(SampicEventBuffer& buffer,
                                 CrateInfoStruct& info,
                                 CrateParamStruct& params,
                                 void* eventBuffer,
                                 ML_Frame* mlFrames,
                                 const SampicCollectorConfig& cfg);

    bool collect() override;

private:
    void populateHit(HitStruct& hit,
                     std::uint32_t hit_index,
                     std::uint32_t waveform_length,
                     std::uint32_t channel_offset);

    const SampicCollectorModeSimulatorConfig& mode_cfg_;
    std::mt19937 rng_;
};

#endif // SAMPIC_COLLECTOR_MODE_SIMULATOR_H
