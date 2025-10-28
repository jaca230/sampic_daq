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

void FrontendEvent::addBank(std::unique_ptr<FrontendEventBank> bank) {
    if (bank)
        banks_.push_back(std::move(bank));
}

std::vector<const FrontendEventBank*> FrontendEvent::banks() const {
    std::vector<const FrontendEventBank*> result;
    result.reserve(banks_.size());
    for (const auto& bank : banks_) {
        result.push_back(bank.get());
    }
    return result;
}

std::vector<FrontendEventBank*> FrontendEvent::banks() {
    std::vector<FrontendEventBank*> result;
    result.reserve(banks_.size());
    for (const auto& bank : banks_) {
        result.push_back(bank.get());
    }
    return result;
}

FrontendEventBank* FrontendEvent::findBankByPrefix(const std::string& prefix) const {
    for (const auto& bank : banks_) {
        if (bank && bank->bankPrefix() == prefix)
            return bank.get();
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
