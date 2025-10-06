#ifndef FRONTEND_EVENT_BUFFER_H
#define FRONTEND_EVENT_BUFFER_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <vector>
#include <memory>
#include "processing/sampic_processing/collector/frontend_event.h"

/// Thread-safe buffer for holding finalized FrontendEvents.
class FrontendEventBuffer {
public:
    explicit FrontendEventBuffer(size_t capacity);

    // Producer
    void push(const std::shared_ptr<FrontendEvent>& ev);

    // Consumer
    std::optional<std::shared_ptr<FrontendEvent>> pop();
    std::optional<std::shared_ptr<FrontendEvent>> latest();
    std::vector<std::shared_ptr<FrontendEvent>> getSince(std::chrono::steady_clock::time_point t);

    // Polling helpers
    bool hasNewSince(std::chrono::steady_clock::time_point t) const;
    bool waitForNew(std::chrono::steady_clock::time_point t,
                    std::chrono::milliseconds timeout);

    size_t size() const;
    bool empty() const;

private:
    size_t capacity_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::pair<std::shared_ptr<FrontendEvent>,
                         std::chrono::steady_clock::time_point>> buffer_;
    std::chrono::steady_clock::time_point last_timestamp_;
};

#endif // FRONTEND_EVENT_BUFFER_H
