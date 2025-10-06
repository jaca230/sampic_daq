#ifndef FRONTEND_EVENT_H
#define FRONTEND_EVENT_H

#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"

class FrontendEvent {
public:
    FrontendEvent() = default;
    virtual ~FrontendEvent();

    void setTimestamp(std::chrono::steady_clock::time_point ts);
    std::chrono::steady_clock::time_point timestamp() const;

    void addBank(const std::shared_ptr<FrontendEventBank>& bank);
    const std::vector<std::shared_ptr<FrontendEventBank>>& banks() const;
    std::vector<std::shared_ptr<FrontendEventBank>>& banks();

    std::shared_ptr<FrontendEventBank> findBankByPrefix(const std::string& prefix) const;
    void clearBanks();

    virtual void finalize();  // optional override

private:
    std::chrono::steady_clock::time_point timestamp_{};
    std::vector<std::shared_ptr<FrontendEventBank>> banks_;
};

#endif // FRONTEND_EVENT_H
