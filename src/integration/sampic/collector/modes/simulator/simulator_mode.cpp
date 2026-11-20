#include "integration/sampic/collector/modes/simulator/simulator_mode.h"

#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/collector/sampic_event.h"

#include <spdlog/spdlog.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cmath>
#include <limits>

SAMPIC_REGISTER_MODE(
    SampicCollectorModeRegistry,
    SampicCollectorModeSimulator,
    SampicCollectorModeSimulatorConfig,
    "simulator",
    "Synthetic SAMPIC events",
    [](const SampicCollectorModeSimulatorConfig& config) {
        if (config.events_per_cycle == 0 || config.hits_per_event == 0 ||
            config.waveform_length == 0) {
            throw std::invalid_argument(
                "event, hit, and waveform counts must be non-zero");
        }
        if (config.noise_rms_adc < 0.0 ||
            config.amplitude_jitter_fraction < 0.0 ||
            config.trigger_jitter_samples < 0.0 ||
            config.punch_through_fraction < 0.0 ||
            config.punch_through_fraction > 1.0 ||
            config.punch_amplitude_fraction < 0.0) {
            throw std::invalid_argument(
                "noise and amplitude jitter must be non-negative");
        }
    });

SampicCollectorModeSimulator::SampicCollectorModeSimulator(
    SampicCollectorModeContext& context,
    SampicCollectorModeSimulatorConfig config)
    : SampicCollectorMode(context),
      mode_cfg_(std::move(config))
{
    hit_time_step_ns_ = std::max(1e-3, mode_cfg_.hit_time_step_ns);
    inter_event_gap_ns_ = std::max(0.0, mode_cfg_.inter_event_gap_ns);
    current_event_time_ns_ = mode_cfg_.start_timestamp_ns;
    baseline_level_ = std::clamp(mode_cfg_.baseline_level, 0.0, 1.0);
    signal_amplitude_ = std::clamp(mode_cfg_.signal_amplitude, 0.0, 1.0);
    tot_value_ns_ = std::max(0.0, mode_cfg_.tot_value_ns);
    random_.seed(mode_cfg_.random_seed);
    noise_ = std::normal_distribution<double>(
        0.0, std::max(0.0, mode_cfg_.noise_rms_adc));
    amplitude_jitter_ = std::normal_distribution<double>(
        1.0, std::max(0.0, mode_cfg_.amplitude_jitter_fraction));
    trigger_jitter_ = std::normal_distribution<double>(
        0.0, std::max(0.0, mode_cfg_.trigger_jitter_samples));

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

    spdlog::trace("SampicSimulator: collect() - events_per_cycle={}, hits_per_event={}",
                  events_per_cycle, hits_per_event);

    for (std::uint32_t ev_idx = 0; ev_idx < events_per_cycle; ++ev_idx) {
        auto ev_data = SampicEvent::makeEventStruct();
        auto* ev_struct = ev_data.get();

        const double event_start_time_ns = current_event_time_ns_;
        constexpr std::uint64_t punch_period = 1000;
        const auto punch_count = static_cast<std::uint64_t>(std::llround(
            mode_cfg_.punch_through_fraction * punch_period));
        // Multiplicative permutation spreads the requested fraction uniformly
        // through time while remaining identical for shared-clock crates.
        const auto event_class =
            (event_sequence_ * 2654435761ULL) % punch_period;
        const bool punch_through = event_class < punch_count;

        // Initialize only the fields we use (avoid memset of entire struct)
        ev_struct->NbOfHitsInEvent = static_cast<int>(hits_per_event);
        ev_struct->TriggerData.NbOfTriggers = 0;
        ev_struct->TriggerData.RawDataSize = 0;

        for (std::uint32_t hit_idx = 0; hit_idx < hits_per_event; ++hit_idx) {
            auto& hit = ev_struct->Hit[hit_idx];
            populateHit(hit, hit_idx, waveform_length, ev_idx,
                        event_start_time_ns, punch_through);
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
        ++event_sequence_;
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
                                               double event_start_time_ns,
                                               bool punch_through)
{
    hit.HitNumber = static_cast<int>(hit_index);
    const auto plane = hit_index % 8;
    const auto strip = (event_sequence_ + channel_offset) % 32;
    const auto global_channel = plane * 32 + strip;
    hit.FeBoardIndex = static_cast<int>(global_channel / 64);
    const auto local_channel = static_cast<int>(global_channel % 64);
    hit.SampicIndex = local_channel / NB_OF_CHANNELS_IN_SAMPIC;
    hit.ChannelIndex = local_channel % NB_OF_CHANNELS_IN_SAMPIC;
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

    const double topology_scale = punch_through
        ? mode_cfg_.punch_amplitude_fraction : 1.0;
    const double pulse_scale = topology_scale *
        std::max(0.0, amplitude_jitter_(random_));
    const double baseline_adc = baseline_level_ * ADC_11BITS_MAX_VALUE;
    const double onset = 17.0 + trigger_jitter_(random_);
    for (std::uint32_t sample = 0; sample < waveform_length; ++sample) {
        const double elapsed = static_cast<double>(sample) - onset;
        const double shape = elapsed > 0.0
            ? std::exp(-elapsed / 6.0) - std::exp(-elapsed / 0.9) : 0.0;
        const double signal_adc = signal_amplitude_ * ADC_11BITS_MAX_VALUE * shape;
        const double adc = std::clamp(
            baseline_adc + pulse_scale * signal_adc + noise_(random_),
            0.0, static_cast<double>(ADC_11BITS_MAX_VALUE));
        const auto raw = static_cast<unsigned short>(std::lround(adc));
        hit.RawDataSamples[sample] = raw;
        hit.OrderedRawDataSamples[sample] = raw;
        hit.CorrectedDataSamples[sample] =
            static_cast<float>(adc / ADC_11BITS_MAX_VALUE);
    }
}

void SampicCollectorModeSimulator::prepareWaveformTemplate()
{
    for (std::uint32_t i = 0; i < kMaxWaveformSamples; ++i) {
        const double elapsed = static_cast<double>(i) - 17.0;
        const double shape = elapsed > 0.0
            ? std::exp(-elapsed / 6.0) - std::exp(-elapsed / 0.9) : 0.0;
        const double normalized = baseline_level_ + signal_amplitude_ * shape;
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
