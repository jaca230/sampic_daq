#include "integration/sampic/collector/sampic_event_buffer.h"
#include <spdlog/spdlog.h>

SampicEventBuffer::SampicEventBuffer(size_t capacity)
    : capacity_(capacity),
      last_timestamp_(std::chrono::steady_clock::time_point::min()) {}

void SampicEventBuffer::push(const std::shared_ptr<SampicEvent>& ev) {
    if (!ev) return;

    std::unique_lock<std::mutex> lock(mtx_);

    if (buffer_.size() >= capacity_) {
        buffer_.pop_front();
    }

    const auto ts = ev->timestamp();
    buffer_.emplace_back(ev, ts);
    last_timestamp_ = ts;
    cv_.notify_all();
}

std::optional<std::shared_ptr<SampicEvent>> SampicEventBuffer::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty())
        return std::nullopt;

    auto ev = buffer_.front().first;
    buffer_.pop_front();

    // Warn if we are discarding an event that was never consumed
    if (ev && !ev->consumed()) {
        spdlog::warn("SampicEventBuffer: popping unconsumed event (timestamp={}us, hits={})",
                     std::chrono::duration_cast<std::chrono::microseconds>(
                         ev->timestamp().time_since_epoch()).count(),
                     ev->numHits());
    }

    return ev;
}

std::optional<std::shared_ptr<SampicEvent>> SampicEventBuffer::latest() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty())
        return std::nullopt;
    return buffer_.back().first;
}

std::vector<std::shared_ptr<SampicEvent>>
SampicEventBuffer::getSince(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<SampicEvent>> result;
    result.reserve(buffer_.size());

    for (auto& [ev, ts] : buffer_) {
        if (ts > t)
            result.push_back(ev);
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
    return cv_.wait_for(lock, timeout, [&] { return last_timestamp_ > t; });
}

size_t SampicEventBuffer::size() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.size();
}

bool SampicEventBuffer::empty() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.empty();
}
