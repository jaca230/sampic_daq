#include "integration/sampic/collector/modes/sampic_collector_mode_simulator.h"

#include "integration/sampic/collector/sampic_event.h"

#include <spdlog/spdlog.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cmath>
#include <limits>

SampicCollectorModeSimulator::SampicCollectorModeSimulator(SampicEventBuffer& buffer,
                                                           CrateInfoStruct& info,
                                                           CrateParamStruct& params,
                                                           void* eventBuffer,
                                                           ML_Frame* mlFrames,
                                                           const SampicCollectorConfig& cfg)
    : SampicCollectorMode(buffer, info, params, eventBuffer, mlFrames, cfg),
      mode_cfg_(cfg.simulator_mode)
{
    hit_time_step_ns_ = std::max(1e-3, mode_cfg_.hit_time_step_ns);
    inter_event_gap_ns_ = std::max(0.0, mode_cfg_.inter_event_gap_ns);
    current_event_time_ns_ = mode_cfg_.start_timestamp_ns;
    baseline_level_ = std::clamp(mode_cfg_.baseline_level, 0.0, 1.0);
    signal_amplitude_ = std::clamp(mode_cfg_.signal_amplitude, 0.0, 1.0);
    tot_value_ns_ = std::max(0.0, mode_cfg_.tot_value_ns);

    prepareWaveformTemplate();

    spdlog::info("SAMPIC simulator mode initialised "
                 "(events_per_cycle={}, hits_per_event={}, waveform={}, "
                 "hit_step_ns={}, inter_event_gap_ns={}, amplitude={}, baseline={})",
                 mode_cfg_.events_per_cycle,
                 mode_cfg_.hits_per_event,
                 mode_cfg_.waveform_length,
                 hit_time_step_ns_,
                 inter_event_gap_ns_,
                 signal_amplitude_,
                 baseline_level_);
}

bool SampicCollectorModeSimulator::collect()
{
    const std::uint32_t events_per_cycle = std::max<std::uint32_t>(1, mode_cfg_.events_per_cycle);
    const std::uint32_t hits_per_event = std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, mode_cfg_.hits_per_event),
        kMaxHitsPerEvent);
    const std::uint32_t waveform_length = clampWaveformLength(mode_cfg_.waveform_length);
    const double event_span_ns =
        hit_time_step_ns_ * static_cast<double>(hits_per_event > 0 ? (hits_per_event - 1) : 0);

    for (std::uint32_t ev_idx = 0; ev_idx < events_per_cycle; ++ev_idx) {
        auto ev_data = SampicEvent::makeEventStruct();
        auto* ev_struct = ev_data.get();

        const double event_start_time_ns = current_event_time_ns_;

        // Initialize only the fields we use (avoid memset of entire struct)
        ev_struct->NbOfHitsInEvent = static_cast<int>(hits_per_event);
        ev_struct->TriggerData.NbOfTriggers = 0;
        ev_struct->TriggerData.RawDataSize = 0;

        for (std::uint32_t hit_idx = 0; hit_idx < hits_per_event; ++hit_idx) {
            auto& hit = ev_struct->Hit[hit_idx];
            populateHit(hit, hit_idx, waveform_length, ev_idx, event_start_time_ns);
        }

        SampicTimingBreakdown timing{};
        timing.prepare = std::chrono::microseconds(0);
        timing.read = std::chrono::microseconds(mode_cfg_.simulate_read_time_us);
        timing.decode = std::chrono::microseconds(0);
        timing.total = timing.prepare + timing.read + timing.decode;

        // Create SampicEvent directly with data using unique_ptr (no refcount overhead)
        auto event = std::make_unique<SampicEvent>(std::move(ev_data), timing, std::chrono::steady_clock::now());

        buffer_.push(std::move(event));

        const double scheduled_gap_ns = std::max(inter_event_gap_ns_, event_span_ns);
        current_event_time_ns_ += scheduled_gap_ns;
        if (current_event_time_ns_ < 0.0)
            current_event_time_ns_ = 0.0;
    }

    if (mode_cfg_.simulate_read_time_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(mode_cfg_.simulate_read_time_us));
    }

    return true;
}

void SampicCollectorModeSimulator::populateHit(HitStruct& hit,
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
    // Zero first element is sufficient if we don't memset the whole array
    adv.TriggerPosition[0] = TRUE;

    // Use memcpy for better performance (compiler can optimize better)
    const std::size_t raw_bytes = waveform_length * sizeof(unsigned short);
    const std::size_t corrected_bytes = waveform_length * sizeof(float);
    std::memcpy(hit.RawDataSamples, raw_waveform_template_.data(), raw_bytes);
    std::memcpy(hit.OrderedRawDataSamples, raw_waveform_template_.data(), raw_bytes);
    std::memcpy(hit.CorrectedDataSamples, corrected_waveform_template_.data(), corrected_bytes);
}

void SampicCollectorModeSimulator::prepareWaveformTemplate()
{
    constexpr double kPi = 3.14159265358979323846;
    const double peak_position = 0.35;
    const double pulse_width = 0.12; // fractional width of gaussian envelope

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

        raw_waveform_template_[i] = static_cast<unsigned short>(
            std::clamp(adc_value, 0.0, adc_scale));
        corrected_waveform_template_[i] = static_cast<float>(clamped);
    }
}

std::uint32_t SampicCollectorModeSimulator::clampWaveformLength(std::uint32_t requested) const
{
    return std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, requested),
        kMaxWaveformSamples);
}
