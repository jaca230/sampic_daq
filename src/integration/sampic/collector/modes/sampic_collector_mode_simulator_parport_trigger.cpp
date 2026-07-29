#include "integration/sampic/collector/modes/sampic_collector_mode_simulator_parport_trigger.h"

#include "integration/sampic/collector/sampic_event.h"

#include <parport_trigger/trigger_client.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include <spdlog/spdlog.h>

SampicCollectorModeSimulatorParportTrigger::SampicCollectorModeSimulatorParportTrigger(
    SampicEventBuffer& buffer,
    CrateInfoStruct& info,
    CrateParamStruct& params,
    void* eventBuffer,
    ML_Frame* mlFrames,
    const SampicCollectorConfig& cfg,
    std::shared_ptr<parport_trigger::TriggerClient> trigger_client)
    : SampicCollectorMode(buffer, info, params, eventBuffer, mlFrames, cfg),
      mode_cfg_(cfg.simulator_pp_trig_mode),
      trigger_client_(std::move(trigger_client))
{
    hit_time_step_ns_ = std::max(1e-3, mode_cfg_.hit_time_step_ns);
    inter_event_gap_ns_ = std::max(0.0, mode_cfg_.inter_event_gap_ns);
    current_event_time_ns_ = mode_cfg_.start_timestamp_ns;
    baseline_level_ = std::clamp(mode_cfg_.baseline_level, 0.0, 1.0);
    signal_amplitude_ = std::clamp(mode_cfg_.signal_amplitude, 0.0, 1.0);
    tot_value_ns_ = std::max(0.0, mode_cfg_.tot_value_ns);

    prepareWaveformTemplate();

    spdlog::info("SAMPIC parport-trigger simulator mode initialised "
                 "(events_per_trigger={}, hits_per_event={}, waveform={}, socket='{}', "
                 "wait_timeout_ms={})",
                 mode_cfg_.events_per_trigger,
                 mode_cfg_.hits_per_event,
                 mode_cfg_.waveform_length,
                 mode_cfg_.socket_path,
                 mode_cfg_.wait_timeout_ms);
}

bool SampicCollectorModeSimulatorParportTrigger::collect()
{
    if (!trigger_client_) {
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true)) {
            spdlog::error("Parport-trigger simulator mode has no trigger client; no events will be produced");
        }
        return true;
    }

    if (!trigger_client_->is_running()) {
        trigger_client_->start();
    }

    const auto timeout_ms =
        std::chrono::milliseconds(static_cast<int>(std::max<std::uint32_t>(1, mode_cfg_.wait_timeout_ms)));
    const auto trigger = trigger_client_->wait_for_event(timeout_ms);
    if (!trigger) {
        return true;
    }

    if (!first_trigger_time_.has_value()) {
        first_trigger_time_ = trigger->trigger_time;
    }

    const auto elapsed = trigger->trigger_time - *first_trigger_time_;
    const double elapsed_ns = std::max(
        0.0,
        std::chrono::duration<double, std::nano>(elapsed).count());
    current_event_time_ns_ = mode_cfg_.start_timestamp_ns + elapsed_ns;
    if (current_event_time_ns_ < 0.0) {
        current_event_time_ns_ = 0.0;
    }

    auto trigger_latency = std::chrono::microseconds(0);
    if (trigger->receive_time >= trigger->trigger_time) {
        trigger_latency = std::chrono::duration_cast<std::chrono::microseconds>(
            trigger->receive_time - trigger->trigger_time);
    }

    const auto events_per_trigger = std::max<std::uint32_t>(1, mode_cfg_.events_per_trigger);
    generateEvents(events_per_trigger, trigger_latency);
    return true;
}

void SampicCollectorModeSimulatorParportTrigger::generateEvents(
    std::uint32_t events_per_trigger,
    std::chrono::microseconds trigger_latency)
{
    const std::uint32_t hits_per_event = std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, mode_cfg_.hits_per_event),
        kMaxHitsPerEvent);
    const std::uint32_t waveform_length = clampWaveformLength(mode_cfg_.waveform_length);
    const double event_span_ns =
        hit_time_step_ns_ * static_cast<double>(hits_per_event > 0 ? (hits_per_event - 1) : 0);

    for (std::uint32_t ev_idx = 0; ev_idx < events_per_trigger; ++ev_idx) {
        auto ev_data = SampicEvent::makeEventStruct();
        auto* ev_struct = ev_data.get();

        const double event_start_time_ns = current_event_time_ns_;

        ev_struct->NbOfHitsInEvent = static_cast<int>(hits_per_event);
        ev_struct->TriggerData.NbOfTriggers = 0;
        ev_struct->TriggerData.RawDataSize = 0;

        for (std::uint32_t hit_idx = 0; hit_idx < hits_per_event; ++hit_idx) {
            auto& hit = ev_struct->Hit[hit_idx];
            populateHit(hit, hit_idx, waveform_length, ev_idx, event_start_time_ns);
        }

        SampicTimingBreakdown timing{};
        timing.prepare = std::chrono::microseconds(0);
        timing.read = trigger_latency;
        timing.decode = std::chrono::microseconds(0);
        timing.total = timing.prepare + timing.read + timing.decode;

        auto event = std::make_unique<SampicEvent>(
            std::move(ev_data), timing, std::chrono::steady_clock::now());
        buffer_.push(std::move(event));

        const double scheduled_gap_ns = std::max(inter_event_gap_ns_, event_span_ns);
        current_event_time_ns_ += scheduled_gap_ns;
        if (current_event_time_ns_ < 0.0) {
            current_event_time_ns_ = 0.0;
        }
    }
}

void SampicCollectorModeSimulatorParportTrigger::populateHit(HitStruct& hit,
                                                             std::uint32_t hit_index,
                                                             std::uint32_t waveform_length,
                                                             std::uint32_t channel_offset,
                                                             double event_start_time_ns)
{
    hit.HitNumber = static_cast<int>(hit_index);
    hit.FeBoardIndex = 0;
    hit.SampicIndex = static_cast<int>((channel_offset + hit_index) % NB_OF_SAMPICS_IN_FE_BOARD);
    hit.ChannelIndex = static_cast<int>((channel_offset + hit_index) % NB_OF_CHANNELS_IN_SAMPIC);
    hit.Channel = hit.SampicIndex * NB_OF_CHANNELS_IN_SAMPIC + hit.ChannelIndex;
    hit.DataSize = static_cast<int>(waveform_length);
    hit.CellInfo = static_cast<int>(hit_index % NB_OF_CHANNELS_IN_SAMPIC);
    hit.FirstCellPhysicalIndex = 0;
    hit.INLCorrected = TRUE;
    hit.ADCCorrected = TRUE;
    hit.ResidualPedestalCorrected = TRUE;

    hit.RawTOTValue = static_cast<int>(tot_value_ns_);
    hit.TOTValue = static_cast<float>(tot_value_ns_);
    hit.Amplitude = static_cast<float>(signal_amplitude_);
    hit.Baseline = static_cast<float>(baseline_level_);
    hit.Peak = hit.Amplitude + hit.Baseline;
    const double hit_time_ns = event_start_time_ns +
        static_cast<double>(hit_index) * hit_time_step_ns_;
    const double clamped_time_ns = std::max(0.0, hit_time_ns);
    hit.TimeIndex = static_cast<float>(clamped_time_ns / std::max(1e-6, hit_time_step_ns_));
    hit.TimeInstant = clamped_time_ns;
    hit.TimeAmplitude = hit.Amplitude;
    hit.FirstCellTimeStamp = hit.TimeInstant;

    auto& adv = hit.AdvancedParams;
    adv.SampicDataHeader = DATA_HEADER;
    adv.FirstTriggerPositionCell = 0;
    adv.TriggerPositionCell = 0;
    adv.PhysicalCell0TimeStamp = hit.TimeInstant;
    adv.TimePhysicalIndex = 0;
    const double max_int = static_cast<double>(std::numeric_limits<int>::max());
    const double ts_a = std::fmod(clamped_time_ns, max_int);
    adv.SampicTimeStampA = static_cast<int>(ts_a);
    const double ts_b = std::fmod(clamped_time_ns + 1.0, max_int);
    adv.SampicTimeStampB = static_cast<int>(ts_b);
    adv.FPGATimeStamp = static_cast<unsigned long long>(clamped_time_ns);
    adv.ADCCounter_LatchedAtEndOfConv = hit_index % ADC_11BITS_MAX_VALUE;
    adv.StartOfADCRamp = 0;
    adv.TriggerPosition[0] = TRUE;

    const std::size_t raw_bytes = waveform_length * sizeof(unsigned short);
    const std::size_t corrected_bytes = waveform_length * sizeof(float);
    std::memcpy(hit.RawDataSamples, raw_waveform_template_.data(), raw_bytes);
    std::memcpy(hit.OrderedRawDataSamples, raw_waveform_template_.data(), raw_bytes);
    std::memcpy(hit.CorrectedDataSamples, corrected_waveform_template_.data(), corrected_bytes);
}

void SampicCollectorModeSimulatorParportTrigger::prepareWaveformTemplate()
{
    constexpr double kPi = 3.14159265358979323846;
    const double peak_position = 0.35;
    const double pulse_width = 0.12;

    for (std::uint32_t i = 0; i < kMaxWaveformSamples; ++i) {
        const double phase = (kMaxWaveformSamples > 1)
                                 ? static_cast<double>(i) / static_cast<double>(kMaxWaveformSamples - 1)
                                 : 0.0;
        const double gaussian = std::exp(-std::pow((phase - peak_position) / pulse_width, 2.0));
        const double oscillation = 0.5 * (1.0 - std::cos(2.0 * kPi * phase));
        const double normalized = baseline_level_ + signal_amplitude_ * gaussian * oscillation;
        const double clamped = std::clamp(normalized, 0.0, 1.0);

        const double adc_scale = static_cast<double>(ADC_11BITS_MAX_VALUE);
        const double adc_value = std::round(clamped * adc_scale);
        raw_waveform_template_[i] = static_cast<unsigned short>(std::clamp(adc_value, 0.0, adc_scale));
        corrected_waveform_template_[i] = static_cast<float>(clamped);
    }
}

std::uint32_t SampicCollectorModeSimulatorParportTrigger::clampWaveformLength(std::uint32_t requested) const
{
    return std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, requested),
        kMaxWaveformSamples);
}
