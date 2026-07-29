#ifndef SAMPIC_COLLECTOR_MODE_SIMULATOR_PP_TRIG_H
#define SAMPIC_COLLECTOR_MODE_SIMULATOR_PP_TRIG_H

#include "integration/sampic/collector/modes/sampic_collector_mode.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>

namespace parport_trigger {
class TriggerClient;
struct TriggerStreamEvent;
}

/**
 * @brief Trigger-driven software-only SAMPIC collector.
 *
 * This mode waits for trigger notifications from parport_trigger and
 * generates synthetic EventStruct payloads only when triggers arrive.
 */
class SampicCollectorModeSimulatorParportTrigger : public SampicCollectorMode {
public:
    static constexpr std::uint32_t kMaxWaveformSamples = MAX_NB_OF_SAMPLES;
    static constexpr std::uint32_t kMaxHitsPerEvent = static_cast<std::uint32_t>(MAX_EXPECTED_FRAMES);

    SampicCollectorModeSimulatorParportTrigger(
        SampicEventBuffer& buffer,
        CrateInfoStruct& info,
        CrateParamStruct& params,
        void* eventBuffer,
        ML_Frame* mlFrames,
        const SampicCollectorConfig& cfg,
        std::shared_ptr<parport_trigger::TriggerClient> trigger_client);

    bool collect() override;

private:
    void populateHit(HitStruct& hit,
                     std::uint32_t hit_index,
                     std::uint32_t waveform_length,
                     std::uint32_t channel_offset,
                     double event_start_time_ns);
    void prepareWaveformTemplate();
    std::uint32_t clampWaveformLength(std::uint32_t requested) const;
    void generateEvents(std::uint32_t events_per_trigger,
                        std::chrono::microseconds trigger_latency);

    const SampicCollectorModeSimulatorParportTriggerConfig& mode_cfg_;
    std::shared_ptr<parport_trigger::TriggerClient> trigger_client_;
    std::optional<std::chrono::system_clock::time_point> first_trigger_time_;
    double current_event_time_ns_{0.0};
    double hit_time_step_ns_{5.0};
    double inter_event_gap_ns_{1'500'000.0};
    double baseline_level_{0.05};
    double signal_amplitude_{0.8};
    double tot_value_ns_{120.0};
    std::array<unsigned short, kMaxWaveformSamples> raw_waveform_template_{};
    std::array<float, kMaxWaveformSamples> corrected_waveform_template_{};
};

#endif // SAMPIC_COLLECTOR_MODE_SIMULATOR_PP_TRIG_H
