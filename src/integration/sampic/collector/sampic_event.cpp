#include "integration/sampic/collector/sampic_event.h"
#include <spdlog/fmt/fmt.h>
#include <mutex>
#include <vector>

namespace {

struct EventStructPool {
    std::mutex mutex;
    std::vector<EventStruct*> free_list;
    std::size_t limit = 1024;

    ~EventStructPool() {
        for (auto* ptr : free_list) {
            delete ptr;
        }
    }
};

EventStructPool& globalPool() {
    static EventStructPool pool;
    return pool;
}

} // namespace

SampicEvent::EventPtr SampicEvent::makeEventStruct() {
    return EventPtr(acquireEventStruct(), &SampicEvent::releaseEventStruct);
}

SampicEvent::SampicEvent(EventPtr data,
                         const SampicTimingBreakdown& timing,
                         std::chrono::steady_clock::time_point ts)
    : data_(data ? std::move(data) : makeEventStruct()),
      timing_(timing),
      timestamp_(ts) {}

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

void SampicEvent::setData(EventPtr data) {
    data_ = data ? std::move(data) : makeEventStruct();
}

EventStruct* SampicEvent::data() {
    return data_.get();
}

const EventStruct* SampicEvent::data() const {
    return data_.get();
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

EventStruct* SampicEvent::acquireEventStruct() {
    auto& pool = globalPool();
    std::lock_guard<std::mutex> lock(pool.mutex);
    if (!pool.free_list.empty()) {
        EventStruct* ptr = pool.free_list.back();
        pool.free_list.pop_back();
        return ptr;
    }
    return new EventStruct{};
}

void SampicEvent::releaseEventStruct(EventStruct* ptr) {
    if (!ptr)
        return;

    auto& pool = globalPool();
    std::lock_guard<std::mutex> lock(pool.mutex);
    if (pool.free_list.size() >= pooledEventLimit()) {
        delete ptr;
    } else {
        pool.free_list.push_back(ptr);
    }
}

std::size_t SampicEvent::pooledEventLimit() {
    return globalPool().limit;
}
