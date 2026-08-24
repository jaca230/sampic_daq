#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_mode.h"
#include "core/registry/mode/mode_auto_registration.h"

#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_trigger_metadata.h"
#include "processing/sampic_processing/collector/frontend_event.h"

#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(
    FrontendCollectorModeRegistry,
    FrontendCollectorModeExternalTrigger,
    FrontendCollectorModeExternalTriggerConfig,
    "external_trigger",
    "External-trigger timestamp windows",
    [](const FrontendCollectorModeExternalTriggerConfig& config) {
        if (config.pre_window_ns < 0 || config.post_window_ns < 0 ||
            config.sampling_frequency_mhz <= 0) {
            throw std::invalid_argument(
                "windows must be non-negative and sampling frequency positive");
        }
    });

FrontendCollectorModeExternalTrigger::FrontendCollectorModeExternalTrigger(
    FrontendCollectorModeContext& context,
    FrontendCollectorModeExternalTriggerConfig config)
    : FrontendCollectorMode(context),
      mode_cfg_(std::move(config)),
      associator_(
          mode_cfg_.hit_time_offset_ns,
          mode_cfg_.pre_window_ns,
          mode_cfg_.post_window_ns,
          mode_cfg_.sampling_frequency_mhz) {
    spdlog::info("External-trigger frontend collector initialized (hit offset={} ns, window=[-{}, +{}] ns)",
                 mode_cfg_.hit_time_offset_ns, mode_cfg_.pre_window_ns, mode_cfg_.post_window_ns);
}

bool FrontendCollectorModeExternalTrigger::collect() {
    if (!sampic_buffer_.waitForNew(last_timestamp_, wait_timeout_)) return true;
    auto events = sampic_buffer_.getSince(last_timestamp_);
    if (events.empty()) return true;
    last_timestamp_ = events.back()->timestamp();
    for (const auto& parent_ref : events) {
        if (!parent_ref || !parent_ref->data()) continue;
        const EventStruct& parent = *parent_ref->data();
        if (parent.TriggerData.NbOfTriggers <= 0) {
            spdlog::debug("External-trigger collector: dropping decoded packet with {} hits and no trigger records",
                          parent.NbOfHitsInEvent);
            continue;
        }

        auto association = associator_.associate(parent);
        for (int trigger_index = 0; trigger_index < parent.TriggerData.NbOfTriggers; ++trigger_index) {
            const double trigger_time = parent.TriggerData.TriggerTimeStamp[trigger_index];
            const double reference_time = trigger_time + mode_cfg_.hit_time_offset_ns;
            auto& assigned_hits =
                association.hits_by_trigger[static_cast<std::size_t>(trigger_index)];
            const uint32_t ambiguous_hits =
                association.ambiguous_hits_by_trigger[
                    static_cast<std::size_t>(trigger_index)];
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
