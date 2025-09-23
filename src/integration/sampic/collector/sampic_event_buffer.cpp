#include "integration/sampic/collector/sampic_event_buffer.h"

SampicEventBuffer::SampicEventBuffer(size_t capacity) : capacity_(capacity) {}

void SampicEventBuffer::push(const TimestampedSampicEvent& ev) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.size() >= capacity_) {
        spdlog::warn("SampicEventBuffer full ({} events). Dropping oldest event.", capacity_);
        buffer_.pop_front(); // drop oldest
    }
    buffer_.push_back(ev);
    cv_.notify_one();
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
    return buffer_.back();
}

std::vector<TimestampedSampicEvent> SampicEventBuffer::getSince(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lock(mtx_);
    std::vector<TimestampedSampicEvent> result;
    for (const auto& ev : buffer_) {
        if (ev.timestamp > t) {
            result.push_back(ev);
        }
    }
    return result;
}

size_t SampicEventBuffer::size() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.size();
}

bool SampicEventBuffer::empty() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.empty();
}
