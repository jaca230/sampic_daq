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

/**
 * @class FrontendEventBuffer
 * @brief Thread-safe FIFO buffer for finalized FrontendEvent objects.
 *
 * Provides blocking and non-blocking access methods for both producers
 * (push) and consumers (pop, getSince, etc.). The buffer maintains
 * timestamp ordering and notifies waiting threads on new arrivals.
 */
class FrontendEventBuffer {
public:
    /**
     * @brief Construct a new FrontendEventBuffer with the given capacity.
     * @param capacity Maximum number of events stored before oldest are dropped.
     */
    explicit FrontendEventBuffer(size_t capacity);

    // ------------------------------------------------------------------
    // Producer interface
    // ------------------------------------------------------------------

    /**
     * @brief Add a new event to the buffer.
     * 
     * If the buffer is full, the oldest event is removed. This function
     * signals any threads waiting on new data.
     *
     * @param ev Shared pointer to the FrontendEvent to push.
     */
    void push(const std::shared_ptr<FrontendEvent>& ev);

    // ------------------------------------------------------------------
    // Consumer interface
    // ------------------------------------------------------------------

    /**
     * @brief Retrieve and remove the oldest event, if available.
     *
     * Logs a warning if the popped event was not marked as consumed.
     *
     * @return Optional shared pointer to the popped FrontendEvent.
     *         Empty if the buffer was empty.
     */
    std::optional<std::shared_ptr<FrontendEvent>> pop();

    /**
     * @brief Retrieve the most recent event without removing it.
     * @return Optional shared pointer to the latest event.
     */
    std::optional<std::shared_ptr<FrontendEvent>> latest();

    /**
     * @brief Retrieve all events newer than a specified timestamp.
     * @param t Timestamp threshold.
     * @return Vector of shared pointers to all newer events.
     */
    std::vector<std::shared_ptr<FrontendEvent>> getSince(std::chrono::steady_clock::time_point t);

    // ------------------------------------------------------------------
    // Polling helpers
    // ------------------------------------------------------------------

    /**
     * @brief Check if newer events exist after a given timestamp.
     * @param t Timestamp threshold.
     * @return True if newer events exist.
     */
    bool hasNewSince(std::chrono::steady_clock::time_point t) const;

    /**
     * @brief Wait until a new event arrives or timeout expires.
     * @param t Reference timestamp to compare against.
     * @param timeout Maximum wait duration.
     * @return True if a new event became available before timeout.
     */
    bool waitForNew(std::chrono::steady_clock::time_point t,
                    std::chrono::milliseconds timeout);

    /**
     * @brief Get the number of events currently stored.
     * @return Current buffer size.
     */
    size_t size() const;

    /**
     * @brief Check if the buffer is empty.
     * @return True if no events are stored.
     */
    bool empty() const;

private:
    size_t capacity_; ///< Maximum number of events before oldest are dropped.
    mutable std::mutex mtx_; ///< Mutex for thread safety.
    std::condition_variable cv_; ///< Condition variable for push notifications.
    std::deque<std::pair<std::shared_ptr<FrontendEvent>,
                         std::chrono::steady_clock::time_point>> buffer_; ///< Stored events and timestamps.
    std::chrono::steady_clock::time_point last_timestamp_{}; ///< Timestamp of last received event.
};

#endif // FRONTEND_EVENT_BUFFER_H
