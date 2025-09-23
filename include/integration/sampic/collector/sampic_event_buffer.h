#ifndef SAMPIC_EVENT_BUFFER_H
#define SAMPIC_EVENT_BUFFER_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <vector>
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

/// Wrapper for EventStruct with timestamp and timing diagnostics
struct TimestampedSampicEvent {
    EventStruct event;
    std::chrono::steady_clock::time_point timestamp;
    SampicEventTiming timing;
};

/// Thread-safe buffer for holding timestamped SAMPIC events
class SampicEventBuffer {
public:
    explicit SampicEventBuffer(size_t capacity);

    void push(const TimestampedSampicEvent& ev);
    std::optional<TimestampedSampicEvent> pop();
    std::optional<TimestampedSampicEvent> latest();
    std::vector<TimestampedSampicEvent> getSince(std::chrono::steady_clock::time_point t);

    size_t size() const;
    bool empty() const;

private:
    size_t capacity_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<TimestampedSampicEvent> buffer_;
};

#endif // SAMPIC_EVENT_BUFFER_H
