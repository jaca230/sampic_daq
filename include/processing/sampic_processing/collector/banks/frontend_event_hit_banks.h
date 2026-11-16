#ifndef FRONTEND_EVENT_HIT_BANKS_H
#define FRONTEND_EVENT_HIT_BANKS_H
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data_verbose.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits_verbose.h"
#include <initializer_list>
#include <set>

// All modes use the same ordering and switches. Disabled banks allocate no payload.
template<class Config>
void addFrontendHitBanks(FrontendEvent& event,
                         const std::vector<std::shared_ptr<SampicEvent>>& parents,
                         const std::vector<const HitStruct*>& hits,
                         const Config& config) {
    const auto add = [&](auto bank, const std::string& prefix) {
        bank->setBankPrefix(prefix);
        event.addBank(std::move(bank));
    };
    if (config.data_bank_enabled) {
        if (config.verbose_banks)
            add(std::make_unique<FrontendEventBankDataVerbose>(parents, hits), config.data_bank_prefix);
        else
            add(std::make_unique<FrontendEventBankData>(hits), config.data_bank_prefix);
    }
    if (config.advanced_bank_enabled) {
        if (config.verbose_banks)
            add(std::make_unique<FrontendEventBankAdvancedHitsVerbose>(hits), config.advanced_bank_prefix);
        else
            add(std::make_unique<FrontendEventBankAdvancedHits>(hits), config.advanced_bank_prefix);
    }
}

template<class Config>
void validateFrontendHitBanks(const Config& config,
    std::initializer_list<std::pair<bool, std::string>> other_banks) {
    std::set<std::string> prefixes;
    const auto validate = [&](bool enabled, const std::string& prefix) {
        if (!enabled) return;
        if (prefix.size() != 2 || !prefixes.insert(prefix).second)
            throw std::invalid_argument("enabled bank prefixes must be unique and contain exactly 2 characters");
    };
    validate(config.data_bank_enabled, config.data_bank_prefix);
    validate(config.advanced_bank_enabled, config.advanced_bank_prefix);
    for (const auto& [enabled, prefix] : other_banks) validate(enabled, prefix);
}
#endif
