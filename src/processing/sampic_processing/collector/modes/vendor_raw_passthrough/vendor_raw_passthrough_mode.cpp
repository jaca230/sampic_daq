#include "processing/sampic_processing/collector/modes/vendor_raw_passthrough/vendor_raw_passthrough_mode.h"

#include "core/registry/mode/mode_auto_registration.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_acquisition_timing.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_ordered_raw.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_vendor_triggers.h"
#include "processing/sampic_processing/collector/frontend_event.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

SAMPIC_REGISTER_MODE(
    FrontendCollectorModeRegistry,
    FrontendCollectorModeVendorRawPassthrough,
    FrontendCollectorModeVendorRawPassthroughConfig,
    "vendor_raw_passthrough",
    "One MIDAS event per vendor event with ordered pre-correction ADC samples",
    [](const FrontendCollectorModeVendorRawPassthroughConfig& config) {
        if (config.wait_timeout_ms == 0) {
            throw std::invalid_argument("wait timeout must be positive");
        }
        if (config.raw_data_bank_prefix.size() != 2 ||
            config.acquisition_timing_bank_prefix.size() != 2 ||
            config.trigger_bank_prefix.size() != 2) {
            throw std::invalid_argument("bank prefixes must contain exactly 2 characters");
        }
        if (config.raw_data_bank_enabled && config.acquisition_timing_bank_enabled &&
            config.raw_data_bank_prefix == config.acquisition_timing_bank_prefix) {
            throw std::invalid_argument("raw-data and timing bank prefixes must differ");
        }
        if (config.acquisition_timing_bank_enabled && config.trigger_bank_enabled &&
            config.acquisition_timing_bank_prefix == config.trigger_bank_prefix)
            throw std::invalid_argument("timing and trigger bank prefixes must differ");
        if (config.raw_data_bank_enabled && config.trigger_bank_enabled &&
            config.raw_data_bank_prefix == config.trigger_bank_prefix) {
            throw std::invalid_argument("raw-data and trigger bank prefixes must differ");
        }
    });

FrontendCollectorModeVendorRawPassthrough::FrontendCollectorModeVendorRawPassthrough(
    FrontendCollectorModeContext& context,
    FrontendCollectorModeVendorRawPassthroughConfig config)
    : FrontendCollectorMode(context),
      mode_cfg_(std::move(config)),
      wait_timeout_(mode_cfg_.wait_timeout_ms) {}

bool FrontendCollectorModeVendorRawPassthrough::collect() {
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
    for (const auto& parent : events) {
        if (!parent || !parent->data()) {
            continue;
        }
        const EventStruct& vendor_event = *parent->data();
        const int hit_count = std::clamp(
            vendor_event.NbOfHitsInEvent, 0,
            static_cast<int>(MAX_EXPECTED_FRAMES));
        std::vector<const HitStruct*> hits;
        hits.reserve(static_cast<std::size_t>(hit_count));
        for (int index = 0; index < hit_count; ++index) {
            hits.push_back(&vendor_event.Hit[index]);
        }

        auto output = std::make_shared<FrontendEvent>(parent->timestamp());
        if (mode_cfg_.raw_data_bank_enabled) {
            auto bank = std::make_unique<FrontendEventBankOrderedRaw>(hits);
            bank->setBankPrefix(mode_cfg_.raw_data_bank_prefix);
            output->addBank(std::move(bank));
        }
        if (mode_cfg_.trigger_bank_enabled) {
            auto bank = std::make_unique<FrontendEventBankVendorTriggers>(
                vendor_event.TriggerData);
            bank->setBankPrefix(mode_cfg_.trigger_bank_prefix);
            output->addBank(std::move(bank));
        }
        if (mode_cfg_.acquisition_timing_bank_enabled) {
            auto bank = std::make_unique<FrontendEventBankAcquisitionTiming>(
                *parent, next_acquisition_id_, hits.size());
            bank->setBankPrefix(mode_cfg_.acquisition_timing_bank_prefix);
            output->addBank(std::move(bank));
        }
        ++next_acquisition_id_;
        output->finalize();
        frontend_buffer_.push(output);
        ++produced_events;
        produced_hits += static_cast<std::size_t>(hit_count);
    }

    sampic_buffer_.pruneUpTo(last_timestamp_);
    if (produced_events != 0) {
        diagnostics_.produced(
            produced_events, produced_hits, frontend_buffer_.size());
    }
    return true;
}
