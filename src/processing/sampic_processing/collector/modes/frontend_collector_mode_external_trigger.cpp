#include "processing/sampic_processing/collector/modes/frontend_collector_mode_external_trigger.h"

#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_trigger_metadata.h"
#include "processing/sampic_processing/collector/frontend_event.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <spdlog/spdlog.h>

FrontendCollectorModeExternalTrigger::FrontendCollectorModeExternalTrigger(
    SampicEventBuffer& sampic_buffer, FrontendEventBuffer& frontend_buffer,
    const FrontendEventCollectorConfig& cfg,
    frontend::collector::FrontendDiagnostics& diagnostics)
    : FrontendCollectorMode(sampic_buffer, frontend_buffer, cfg, diagnostics),
      mode_cfg_(cfg.external_trigger_mode) {
    if (mode_cfg_.pre_window_ns < 0 || mode_cfg_.post_window_ns < 0 ||
        mode_cfg_.sampling_frequency_mhz <= 0) {
        throw std::invalid_argument("External-trigger collector windows must be non-negative and sampling frequency positive");
    }
    spdlog::info("External-trigger frontend collector initialized (hit offset={} ns, window=[-{}, +{}] ns)",
                 mode_cfg_.hit_time_offset_ns, mode_cfg_.pre_window_ns, mode_cfg_.post_window_ns);
}

bool FrontendCollectorModeExternalTrigger::collect() {
    if (!sampic_buffer_.waitForNew(last_timestamp_, wait_timeout_)) return true;
    auto events = sampic_buffer_.getSince(last_timestamp_);
    if (events.empty()) return true;
    last_timestamp_ = events.back()->timestamp();
    const double sample_period_ns = 1000.0 / mode_cfg_.sampling_frequency_mhz;

    for (const auto& parent_ref : events) {
        if (!parent_ref || !parent_ref->data()) continue;
        const EventStruct& parent = *parent_ref->data();
        if (parent.TriggerData.NbOfTriggers <= 0) {
            spdlog::debug("External-trigger collector: dropping decoded packet with {} hits and no trigger records",
                          parent.NbOfHitsInEvent);
            continue;
        }

        for (int trigger_index = 0; trigger_index < parent.TriggerData.NbOfTriggers; ++trigger_index) {
            const double trigger_time = parent.TriggerData.TriggerTimeStamp[trigger_index];
            const double reference_time = trigger_time + mode_cfg_.hit_time_offset_ns;
            std::vector<const HitStruct*> assigned_hits;
            assigned_hits.reserve(parent.NbOfHitsInEvent);
            uint32_t ambiguous_hits = 0;

            for (int hit_index = 0; hit_index < parent.NbOfHitsInEvent; ++hit_index) {
                const HitStruct& hit = parent.Hit[hit_index];
                const double hit_time = hit.FirstCellTimeStamp +
                    hit.AdvancedParams.FirstTriggerPositionCell * sample_period_ns;
                const double delta = hit_time - reference_time;
                if (delta < -mode_cfg_.pre_window_ns || delta > mode_cfg_.post_window_ns) continue;

                int nearest_index = trigger_index;
                double nearest_distance = std::abs(delta);
                for (int other = 0; other < parent.TriggerData.NbOfTriggers; ++other) {
                    const double other_reference = parent.TriggerData.TriggerTimeStamp[other] +
                                                   mode_cfg_.hit_time_offset_ns;
                    const double distance = std::abs(hit_time - other_reference);
                    if (distance < nearest_distance) {
                        nearest_distance = distance;
                        nearest_index = other;
                    }
                }
                if (nearest_index != trigger_index) continue;
                const int matches = std::count_if(
                    parent.TriggerData.TriggerTimeStamp,
                    parent.TriggerData.TriggerTimeStamp + parent.TriggerData.NbOfTriggers,
                    [&](double ts) {
                        const double d = hit_time - (ts + mode_cfg_.hit_time_offset_ns);
                        return d >= -mode_cfg_.pre_window_ns && d <= mode_cfg_.post_window_ns;
                    });
                if (matches > 1) ++ambiguous_hits;
                assigned_hits.push_back(&hit);
            }
            if (assigned_hits.empty() && !mode_cfg_.emit_triggers_without_hits) continue;

            auto frontend_event = std::make_shared<FrontendEvent>(parent_ref->timestamp());
            std::vector<std::shared_ptr<SampicEvent>> parents{parent_ref};
            auto data_bank = std::make_unique<FrontendEventBankData>(std::move(parents), assigned_hits);
            data_bank->setBankPrefix(mode_cfg_.data_bank_prefix);
            frontend_event->addBank(std::move(data_bank));

            FrontendEventBankTriggerMetadata::Record metadata{
                static_cast<uint32_t>(parent.TriggerData.TriggerIDFromFPGA[trigger_index]),
                parent.TriggerData.TriggerIDFromExtTrig[trigger_index],
                static_cast<uint32_t>(trigger_index),
                static_cast<uint32_t>(assigned_hits.size()), ambiguous_hits,
                trigger_time, reference_time};
            auto trigger_bank = std::make_unique<FrontendEventBankTriggerMetadata>(metadata);
            trigger_bank->setBankPrefix(mode_cfg_.trigger_metadata_bank_prefix);
            frontend_event->addBank(std::move(trigger_bank));

            std::vector<SampicEvent*> timing_parents{parent_ref.get()};
            auto timing_bank = std::make_unique<FrontendEventBankEventTiming>(
                parent_ref->timestamp(), static_cast<uint32_t>(assigned_hits.size()), timing_parents);
            timing_bank->setBankPrefix(mode_cfg_.event_timing_bank_prefix);
            frontend_event->addBank(std::move(timing_bank));
            frontend_buffer_.push(std::move(frontend_event));
        }
    }
    sampic_buffer_.pruneUpTo(last_timestamp_);
    return true;
}
