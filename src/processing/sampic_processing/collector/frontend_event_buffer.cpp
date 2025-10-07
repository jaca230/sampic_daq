#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include <spdlog/spdlog.h>

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------

FrontendEventBuffer::FrontendEventBuffer(size_t capacity)
    : capacity_(capacity),
      last_timestamp_(std::chrono::steady_clock::time_point::min()) {}

// ------------------------------------------------------------------
// Producer interface
// ------------------------------------------------------------------

void FrontendEventBuffer::push(const std::shared_ptr<FrontendEvent>& ev) {
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

// ------------------------------------------------------------------
// Consumer interface
// ------------------------------------------------------------------

std::optional<std::shared_ptr<FrontendEvent>> FrontendEventBuffer::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty())
        return std::nullopt;

    auto ev = buffer_.front().first;
    buffer_.pop_front();

    // Warn if we are discarding an unconsumed event
    if (ev && !ev->consumed()) {
        spdlog::warn("FrontendEventBuffer: popping unconsumed event (banks={}, size={} B)",
                     ev->numBanks(), ev->totalDataSize());
    }

    return ev;
}

std::optional<std::shared_ptr<FrontendEvent>> FrontendEventBuffer::latest() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.empty())
        return std::nullopt;
    return buffer_.back().first;
}

std::vector<std::shared_ptr<FrontendEvent>>
FrontendEventBuffer::getSince(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<FrontendEvent>> result;
    result.reserve(buffer_.size());

    for (auto& [ev, ts] : buffer_) {
        if (ts > t)
            result.push_back(ev);
    }
    return result;
}

// ------------------------------------------------------------------
// Polling helpers
// ------------------------------------------------------------------

bool FrontendEventBuffer::hasNewSince(std::chrono::steady_clock::time_point t) const {
    std::unique_lock<std::mutex> lock(mtx_);
    return last_timestamp_ > t;
}

bool FrontendEventBuffer::waitForNew(std::chrono::steady_clock::time_point t,
                                     std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, timeout, [&] { return last_timestamp_ > t; });
}

size_t FrontendEventBuffer::size() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.size();
}

bool FrontendEventBuffer::empty() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return buffer_.empty();
}
