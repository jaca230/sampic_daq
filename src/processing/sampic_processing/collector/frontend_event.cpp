#include "processing/sampic_processing/collector/frontend_event.h"
#include <spdlog/fmt/fmt.h>

// ------------------------------------------------------------------
// Construction / Destruction
// ------------------------------------------------------------------

FrontendEvent::FrontendEvent(std::chrono::steady_clock::time_point ts)
    : timestamp_(ts) {}

FrontendEvent::~FrontendEvent() = default;

// ------------------------------------------------------------------
// Timestamp accessors
// ------------------------------------------------------------------

void FrontendEvent::setTimestamp(std::chrono::steady_clock::time_point ts) {
    timestamp_ = ts;
}

std::chrono::steady_clock::time_point FrontendEvent::timestamp() const {
    return timestamp_;
}

// ------------------------------------------------------------------
// Bank management
// ------------------------------------------------------------------

void FrontendEvent::addBank(const std::shared_ptr<FrontendEventBank>& bank) {
    if (bank)
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
        if (bank && bank->bankPrefix() == prefix)
            return bank;
    }
    return nullptr;
}

void FrontendEvent::clearBanks() {
    banks_.clear();
}

size_t FrontendEvent::numBanks() const {
    return banks_.size();
}

size_t FrontendEvent::totalDataSize() const {
    size_t total = 0;
    for (const auto& b : banks_) {
        if (b)
            total += b->size();
    }
    return total;
}

// ------------------------------------------------------------------
// Consumption state
// ------------------------------------------------------------------

void FrontendEvent::markConsumed(bool value) {
    consumed_ = value;
}

bool FrontendEvent::consumed() const {
    return consumed_;
}

// ------------------------------------------------------------------
// Finalization hook
// ------------------------------------------------------------------

void FrontendEvent::finalize() {
    // Default implementation does nothing.
}
