#ifndef SAMPIC_EVENT_BUFFER_H
#define SAMPIC_EVENT_BUFFER_H

#include "integration/sampic/collector/sampic_event.h"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <vector>
#include <memory>

/**
 * @brief Thread-safe buffer for holding SampicEvent objects.
 *
 * Provides blocking and non-blocking access methods for both
 * producers (push) and consumers (pop, getSince, etc.).
 * Mirrors the design of FrontendEventBuffer.
 */
class SampicEventBuffer {
public:
    explicit SampicEventBuffer(size_t capacity);

    /** @brief Add a new event to the buffer. Drops oldest if full. */
    void push(const std::shared_ptr<SampicEvent>& ev);

    /** @brief Retrieve and remove the oldest event, if available. */
    std::optional<std::shared_ptr<SampicEvent>> pop();

    /** @brief Get a reference to the most recent event without removing it. */
    std::optional<std::shared_ptr<SampicEvent>> latest();

    /** @brief Retrieve all events newer than a specified timestamp. */
    std::vector<std::shared_ptr<SampicEvent>>
    getSince(std::chrono::steady_clock::time_point t);

    /** @brief Check if newer events exist after a given timestamp. */
    bool hasNewSince(std::chrono::steady_clock::time_point t) const;

    /**
     * @brief Wait until a new event is available or the timeout expires.
     * @return true if a new event became available before timeout.
     */
    bool waitForNew(std::chrono::steady_clock::time_point t,
                    std::chrono::milliseconds timeout);

    /** @brief Number of events currently in the buffer. */
    size_t size() const;

    /** @brief Return true if the buffer is empty. */
    bool empty() const;

private:
    size_t capacity_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::pair<std::shared_ptr<SampicEvent>,
                         std::chrono::steady_clock::time_point>> buffer_;
    std::chrono::steady_clock::time_point last_timestamp_;
};

#endif // SAMPIC_EVENT_BUFFER_H
