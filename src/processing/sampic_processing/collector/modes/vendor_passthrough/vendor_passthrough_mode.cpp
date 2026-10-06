#include "processing/sampic_processing/collector/modes/vendor_passthrough/vendor_passthrough_mode.h"

#include "core/registry/mode/mode_auto_registration.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_vendor_triggers.h"
#include "processing/sampic_processing/collector/frontend_event.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

SAMPIC_REGISTER_MODE(
    FrontendCollectorModeRegistry,
    FrontendCollectorModeVendorPassthrough,
    FrontendCollectorModeVendorPassthroughConfig,
    "vendor_passthrough",
    "One MIDAS event per decoded vendor event",
    [](const FrontendCollectorModeVendorPassthroughConfig& config) {
        if (config.wait_timeout_ms == 0) {
            throw std::invalid_argument("wait timeout must be positive");
        }
    });

FrontendCollectorModeVendorPassthrough::FrontendCollectorModeVendorPassthrough(
    FrontendCollectorModeContext& context,
    FrontendCollectorModeVendorPassthroughConfig config)
    : FrontendCollectorMode(context),
      mode_cfg_(std::move(config)),
      wait_timeout_(mode_cfg_.wait_timeout_ms) {
    spdlog::info(
        "Vendor-pass-through frontend collector initialized "
        "(wait_timeout_ms={}, include_trigger_records={})",
        mode_cfg_.wait_timeout_ms,
        mode_cfg_.include_trigger_records);
}

bool FrontendCollectorModeVendorPassthrough::collect() {
    if (!sampic_buffer_.waitForNew(last_timestamp_, wait_timeout_)) {
        return true;
    }

    auto events = sampic_buffer_.getSince(last_timestamp_);
    if (events.empty()) {
        return true;
    }
    last_timestamp_ = events.back()->timestamp();

    std::size_t produced_events = 0;
    std::size_t produced_hits = 0;
    for (const auto& parent_ref : events) {
        if (!parent_ref || !parent_ref->data()) {
            continue;
        }

        const EventStruct& vendor_event = *parent_ref->data();
        const int hit_count = std::clamp(
            vendor_event.NbOfHitsInEvent,
            0,
            static_cast<int>(MAX_EXPECTED_FRAMES));
        std::vector<const HitStruct*> hits;
        hits.reserve(static_cast<std::size_t>(hit_count));
        for (int index = 0; index < hit_count; ++index) {
            hits.push_back(&vendor_event.Hit[index]);
        }

        auto frontend_event =
            std::make_shared<FrontendEvent>(parent_ref->timestamp());
        if (!hits.empty()) {
            std::vector<std::shared_ptr<SampicEvent>> parents{parent_ref};
            auto data_bank =
                std::make_unique<FrontendEventBankData>(std::move(parents), hits);
            data_bank->setBankPrefix(mode_cfg_.data_bank_prefix);
            frontend_event->addBank(std::move(data_bank));
        }

        if (mode_cfg_.include_trigger_records) {
            auto trigger_bank = std::make_unique<FrontendEventBankVendorTriggers>(
                vendor_event.TriggerData);
            trigger_bank->setBankPrefix(mode_cfg_.trigger_bank_prefix);
            frontend_event->addBank(std::move(trigger_bank));
        }

        std::vector<SampicEvent*> timing_parents{parent_ref.get()};
        auto timing_bank = std::make_unique<FrontendEventBankEventTiming>(
            parent_ref->timestamp(),
            static_cast<std::uint32_t>(hit_count),
            timing_parents);
        timing_bank->setBankPrefix(mode_cfg_.event_timing_bank_prefix);
        frontend_event->addBank(std::move(timing_bank));
        frontend_event->finalize();
        frontend_buffer_.push(frontend_event);

        ++produced_events;
        produced_hits += static_cast<std::size_t>(hit_count);
    }

    sampic_buffer_.pruneUpTo(last_timestamp_);
    if (produced_events > 0) {
        diagnostics_.produced(
            produced_events, produced_hits, frontend_buffer_.size());
    }
    return true;
}
