#include "integration/sampic/collector/sampic_event_buffer.h"

SampicEventBuffer::SampicEventBuffer(size_t capacity)
    : capacity_(capacity),
      last_timestamp_(std::chrono::steady_clock::time_point::min()) {}

void SampicEventBuffer::push(const TimestampedSampicEvent& ev) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.size() >= capacity_) {
        const auto& oldest = buffer_.front();
        if (!oldest.consumed) {
            auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             oldest.timestamp.time_since_epoch())
                             .count();
            spdlog::warn(
                "SampicEventBuffer full ({} events). Dropping UNCONSUMED event "
                "with timestamp={} ns",
                capacity_, ts_ns);
        } else {
            spdlog::debug("SampicEventBuffer full ({} events). Dropping consumed event.", capacity_);
        }
        buffer_.pop_front();
    }
    buffer_.push_back(ev);
    last_timestamp_ = ev.timestamp;
    cv_.notify_all();
}

std::optional<TimestampedSampicEvent> SampicEventBuffer::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty()) {
        return std::nullopt;
    }
    auto ev = buffer_.front();
    buffer_.pop_front();
    return ev;
}

std::optional<TimestampedSampicEvent> SampicEventBuffer::latest() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty()) {
        return std::nullopt;
    }
    auto& ev = buffer_.back();
    if (ev.consumed) {
        spdlog::warn("SampicEventBuffer: latest() returning already-consumed event");
    }
    ev.consumed = true;
    return ev;
}


std::vector<TimestampedSampicEvent>
SampicEventBuffer::getSince(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lock(mtx_);
    std::vector<TimestampedSampicEvent> result;
    for (auto& ev : buffer_) {
        if (ev.timestamp > t) {
            if (ev.consumed) {
                spdlog::warn("SampicEventBuffer: event already consumed but returned by getSince()");
            }
            ev.consumed = true;
            result.push_back(ev);
        }
    }
    return result;
}

bool SampicEventBuffer::hasNewSince(std::chrono::steady_clock::time_point t) const {
    std::unique_lock<std::mutex> lock(mtx_);
    return last_timestamp_ > t;
}

bool SampicEventBuffer::waitForNew(std::chrono::steady_clock::time_point t,
                                   std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, timeout, [&] {
        return last_timestamp_ > t;
    });
}

size_t SampicEventBuffer::size() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.size();
}

bool SampicEventBuffer::empty() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.empty();
}
