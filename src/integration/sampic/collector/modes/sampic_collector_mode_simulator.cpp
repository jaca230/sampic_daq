#include "integration/sampic/collector/modes/sampic_collector_mode_simulator.h"

#include "integration/sampic/collector/sampic_event.h"

#include <spdlog/spdlog.h>
#include <chrono>
#include <cstring>
#include <thread>

namespace {
constexpr std::uint32_t kMaxWaveformSamples = MAX_NB_OF_SAMPLES;
constexpr std::uint32_t kMaxHitsPerEvent = static_cast<std::uint32_t>(MAX_EXPECTED_FRAMES);
}

SampicCollectorModeSimulator::SampicCollectorModeSimulator(SampicEventBuffer& buffer,
                                                           CrateInfoStruct& info,
                                                           CrateParamStruct& params,
                                                           void* eventBuffer,
                                                           ML_Frame* mlFrames,
                                                           const SampicCollectorConfig& cfg)
    : SampicCollectorMode(buffer, info, params, eventBuffer, mlFrames, cfg),
      mode_cfg_(cfg.simulator_mode),
      rng_(std::random_device{}())
{
    spdlog::info("SAMPIC simulator mode initialised (events_per_cycle={}, hits_per_event={}, waveform={})",
                 mode_cfg_.events_per_cycle,
                 mode_cfg_.hits_per_event,
                 mode_cfg_.waveform_length);
}

bool SampicCollectorModeSimulator::collect()
{
    const std::uint32_t events_per_cycle = std::max<std::uint32_t>(1, mode_cfg_.events_per_cycle);
    const std::uint32_t hits_per_event = std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, mode_cfg_.hits_per_event),
        kMaxHitsPerEvent);
    const std::uint32_t waveform_length = std::min<std::uint32_t>(
        std::max<std::uint32_t>(1, mode_cfg_.waveform_length),
        kMaxWaveformSamples);

    for (std::uint32_t ev_idx = 0; ev_idx < events_per_cycle; ++ev_idx) {
        auto ev_data = std::make_shared<EventStruct>();
        std::memset(ev_data.get(), 0, sizeof(EventStruct));
        ev_data->NbOfHitsInEvent = static_cast<int>(hits_per_event);

        for (std::uint32_t hit_idx = 0; hit_idx < hits_per_event; ++hit_idx) {
            auto& hit = ev_data->Hit[hit_idx];
            populateHit(hit, hit_idx, waveform_length, ev_idx);
        }

        SampicTimingBreakdown timing{};
        timing.prepare = std::chrono::microseconds(0);
        timing.read = std::chrono::microseconds(mode_cfg_.simulate_read_time_us);
        timing.decode = std::chrono::microseconds(0);
        timing.total = timing.prepare + timing.read + timing.decode;

        auto event = buffer_.acquire();
        event->setData(ev_data);
        event->setTiming(timing);
        event->setTimestamp(std::chrono::steady_clock::now());

        buffer_.push(std::move(event));
    }

    if (mode_cfg_.simulate_read_time_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(mode_cfg_.simulate_read_time_us));
    }

    return true;
}

void SampicCollectorModeSimulator::populateHit(HitStruct& hit,
                                               std::uint32_t hit_index,
                                               std::uint32_t waveform_length,
                                               std::uint32_t channel_offset)
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

    hit.RawTOTValue = 100 + static_cast<int>(hit_index);
    hit.TOTValue = 5.0f + static_cast<float>(hit_index) * 0.1f;
    hit.Amplitude = 0.8f;
    hit.Baseline = 0.1f;
    hit.Peak = hit.Amplitude + hit.Baseline;
    hit.TimeIndex = static_cast<float>(hit_index);
    hit.TimeInstant = static_cast<double>(hit_index) * 1.0;
    hit.TimeAmplitude = hit.Amplitude;
    hit.FirstCellTimeStamp = hit.TimeInstant;

    auto& adv = hit.AdvancedParams;
    adv.SampicDataHeader = DATA_HEADER;
    adv.FirstTriggerPositionCell = 0;
    adv.TriggerPositionCell = 0;
    adv.PhysicalCell0TimeStamp = hit.TimeInstant;
    adv.SampicTimeStampA = hit_index;
    adv.SampicTimeStampB = hit_index + 1;
    adv.FPGATimeStamp = static_cast<unsigned long long>(hit.TimeInstant);
    adv.ADCCounter_LatchedAtEndOfConv = hit_index % ADC_11BITS_MAX_VALUE;
    adv.StartOfADCRamp = 0;
    std::memset(adv.TriggerPosition, 0, sizeof(adv.TriggerPosition));
    adv.TriggerPosition[0] = TRUE;

    for (std::uint32_t sample = 0; sample < waveform_length; ++sample) {
        const float val = hit.Baseline + hit.Amplitude * static_cast<float>(sample) / waveform_length;
        const unsigned short adc_val = static_cast<unsigned short>(std::min<float>(
            ADC_11BITS_MAX_VALUE,
            std::max<float>(0.0f, val * ADC_11BITS_MAX_VALUE)));
        hit.RawDataSamples[sample] = adc_val;
        hit.OrderedRawDataSamples[sample] = adc_val;
        hit.CorrectedDataSamples[sample] = val;
    }
}
