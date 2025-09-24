#ifndef SAMPIC_EVENT_BUFFER_H
#define SAMPIC_EVENT_BUFFER_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

/// Timing breakdown for diagnostics
struct SampicEventTiming {
    std::chrono::microseconds prepare_duration{0};
    std::chrono::microseconds read_duration{0};
    std::chrono::microseconds decode_duration{0};
    std::chrono::microseconds total_duration{0};
};

/// Wrapper: shared_ptr to EventStruct + timestamp + timing
struct TimestampedSampicEvent {
    std::shared_ptr<EventStruct> event;
    std::chrono::steady_clock::time_point timestamp;
    SampicEventTiming timing;
    bool consumed{false};
};

/// Thread-safe buffer for holding timestamped SAMPIC events
class SampicEventBuffer {
public:
    explicit SampicEventBuffer(size_t capacity);

    // Producer
    void push(const std::shared_ptr<EventStruct>& ev, 
              const SampicEventTiming& timing);

    // Consumer
    std::optional<TimestampedSampicEvent> pop();        ///< destructive
    std::optional<TimestampedSampicEvent> latest();     ///< peek latest, marks consumed
    std::vector<TimestampedSampicEvent> getSince(std::chrono::steady_clock::time_point t);

    // Polling helpers
    bool hasNewSince(std::chrono::steady_clock::time_point t) const;
    bool waitForNew(std::chrono::steady_clock::time_point t,
                    std::chrono::milliseconds timeout);

    // Info
    size_t size() const;
    bool empty() const;

private:
    size_t capacity_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<TimestampedSampicEvent> buffer_;
    std::chrono::steady_clock::time_point last_timestamp_;
};

#endif // SAMPIC_EVENT_BUFFER_H
