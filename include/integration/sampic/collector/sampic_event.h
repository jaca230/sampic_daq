#ifndef SAMPIC_EVENT_H
#define SAMPIC_EVENT_H

#include <memory>
#include <chrono>
#include <string>
#include <cstddef>
#include <cstdint>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

/// Timing breakdown for diagnostics and performance monitoring.
struct SampicTimingBreakdown {
    std::chrono::microseconds prepare{0};
    std::chrono::microseconds read{0};
    std::chrono::microseconds decode{0};
    std::chrono::microseconds total{0};
    uint32_t acquisition_retries{0};
};

/// Represents a single low-level SAMPIC event with timing metadata.
class SampicEvent {
public:
    struct EventStructDeleter {
        void operator()(EventStruct* ptr) const;
    };
    using EventPtr = std::unique_ptr<EventStruct, EventStructDeleter>;

    /// Allocate a zero-initialized EventStruct.
    static EventPtr makeEventStruct();

    SampicEvent() = default;
    SampicEvent(EventPtr data,
                const SampicTimingBreakdown& timing,
                std::chrono::steady_clock::time_point ts);
    virtual ~SampicEvent();

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------
    void setTimestamp(std::chrono::steady_clock::time_point ts);
    std::chrono::steady_clock::time_point timestamp() const;

    void setData(EventPtr data);
    EventStruct* data();
    const EventStruct* data() const;

    void setTiming(const SampicTimingBreakdown& timing);
    const SampicTimingBreakdown& timing() const;

    // ------------------------------------------------------------------
    // Consumption state
    // ------------------------------------------------------------------
    void markConsumed(bool value = true);
    bool consumed() const;

    // ------------------------------------------------------------------
    // Derived Info
    // ------------------------------------------------------------------
    int numHits() const;
    std::string summary() const;

    // ------------------------------------------------------------------
    // Optional finalization hook (symmetry with FrontendEvent)
    // ------------------------------------------------------------------
    virtual void finalize();

private:
    EventPtr data_;
    SampicTimingBreakdown timing_{};
    std::chrono::steady_clock::time_point timestamp_{};
    bool consumed_{false};  ///< Whether this event has been consumed by a downstream processor
};

#endif // SAMPIC_EVENT_H
