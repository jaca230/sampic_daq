#ifndef SAMPIC_COLLECTOR_MODE_SIMULATOR_H
#define SAMPIC_COLLECTOR_MODE_SIMULATOR_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

#include <random>
#include <array>

/**
 * @brief Software-only SAMPIC collector used for throughput testing.
 *
 * The simulator bypasses the vendor library and produces synthetic
 * EventStruct payloads that mirror the structure expected by the rest of the
 * DAQ pipeline.
 */
class SampicCollectorModeSimulator : public SampicCollectorMode {
public:
    static constexpr std::uint32_t kMaxWaveformSamples = MAX_NB_OF_SAMPLES;
    static constexpr std::uint32_t kMaxHitsPerEvent = static_cast<std::uint32_t>(MAX_EXPECTED_FRAMES);

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
                     std::uint32_t channel_offset,
                     double event_start_time_ns);

    double sampleJitter(double sigma_ns);
    void prepareWaveformTemplate();

    const SampicCollectorModeSimulatorConfig& mode_cfg_;
    std::mt19937 rng_;
    double current_event_time_ns_{0.0};
    double hit_time_step_ns_{5.0};
    double inter_event_gap_ns_{1'500'000.0};
    double event_jitter_ns_{0.0};
    double hit_jitter_ns_{0.0};
    double baseline_level_{0.05};
    double signal_amplitude_{0.8};
    double tot_value_ns_{120.0};
    std::array<unsigned short, kMaxWaveformSamples> raw_waveform_template_{};
    std::array<float, kMaxWaveformSamples> corrected_waveform_template_{};
};

#endif // SAMPIC_COLLECTOR_MODE_SIMULATOR_H
