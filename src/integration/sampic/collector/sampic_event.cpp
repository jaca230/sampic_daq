#include "integration/sampic/collector/sampic_event.h"
#include <spdlog/fmt/fmt.h>

SampicEvent::SampicEvent(std::shared_ptr<EventStruct> data,
                         const SampicTimingBreakdown& timing,
                         std::chrono::steady_clock::time_point ts)
    : data_(std::move(data)), timing_(timing), timestamp_(ts) {}

SampicEvent::~SampicEvent() = default;

// ------------------------------------------------------------------
// Accessors
// ------------------------------------------------------------------
void SampicEvent::setTimestamp(std::chrono::steady_clock::time_point ts) {
    timestamp_ = ts;
}

std::chrono::steady_clock::time_point SampicEvent::timestamp() const {
    return timestamp_;
}

void SampicEvent::setData(const std::shared_ptr<EventStruct>& data) {
    data_ = data;
}

const std::shared_ptr<EventStruct>& SampicEvent::data() const {
    return data_;
}

void SampicEvent::setTiming(const SampicTimingBreakdown& timing) {
    timing_ = timing;
}

const SampicTimingBreakdown& SampicEvent::timing() const {
    return timing_;
}

// ------------------------------------------------------------------
// Consumption state
// ------------------------------------------------------------------
void SampicEvent::markConsumed(bool value) {
    consumed_ = value;
}

bool SampicEvent::consumed() const {
    return consumed_;
}

// ------------------------------------------------------------------
// Derived Info
// ------------------------------------------------------------------
int SampicEvent::numHits() const {
    return data_ ? data_->NbOfHitsInEvent : 0;
}

std::string SampicEvent::summary() const {
    uint64_t t_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp_.time_since_epoch()).count());

    return fmt::format(
        "SAMPIC Event @ {} us | hits={} | prepare={}us | read={}us | decode={}us | total={}us | consumed={}",
        t_us,
        numHits(),
        timing_.prepare.count(),
        timing_.read.count(),
        timing_.decode.count(),
        timing_.total.count(),
        consumed_ ? "true" : "false");
}

// ------------------------------------------------------------------
// Optional hook
// ------------------------------------------------------------------
void SampicEvent::finalize() {
    // No-op by default.
    // Can be overridden if additional derived metadata or validation is needed.
}
