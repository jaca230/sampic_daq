#include "processing/sampic_processing/collector/frontend_event.h"

FrontendEvent::~FrontendEvent() = default;

void FrontendEvent::setTimestamp(std::chrono::steady_clock::time_point ts) {
    timestamp_ = ts;
}

std::chrono::steady_clock::time_point FrontendEvent::timestamp() const {
    return timestamp_;
}

void FrontendEvent::addBank(const std::shared_ptr<FrontendEventBank>& bank) {
    banks_.push_back(bank);
}

const std::vector<std::shared_ptr<FrontendEventBank>>& FrontendEvent::banks() const {
    return banks_;
}

std::vector<std::shared_ptr<FrontendEventBank>>& FrontendEvent::banks() {
    return banks_;
}

std::shared_ptr<FrontendEventBank> FrontendEvent::findBankByPrefix(const std::string& prefix) const {
    for (auto& bank : banks_) {
        if (bank->bankPrefix() == prefix)
            return bank;
    }
    return nullptr;
}

void FrontendEvent::clearBanks() {
    banks_.clear();
}

void FrontendEvent::finalize() {
    // Do nothing by default
    // Override in derived classes if needed
    // Just here in case we need to corroborate
    // data across multiple banks in the future
}
