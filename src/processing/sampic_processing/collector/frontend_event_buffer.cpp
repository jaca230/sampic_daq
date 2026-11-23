#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------

FrontendEventBuffer::FrontendEventBuffer(size_t capacity)
    : capacity_(capacity),
      last_timestamp_(std::chrono::steady_clock::time_point::min()) {
    if (!capacity_) throw std::invalid_argument("queue capacity must be positive");
}

// ------------------------------------------------------------------
// Producer interface
// ------------------------------------------------------------------

void FrontendEventBuffer::push(const std::shared_ptr<FrontendEvent>& ev) {
    if (!ev) return;

    std::unique_lock<std::mutex> lock(mtx_);

    if (buffer_.size() >= capacity_) {
        throw std::overflow_error("Frontend output queue overflow (capacity=" +
            std::to_string(capacity_) + "): incoming event cannot be queued; data incomplete");
    }

    const auto ts = ev->timestamp();
    buffer_.emplace_back(ev, ts);
    last_timestamp_ = ts;
    cv_.notify_all();
}

bool FrontendEventBuffer::tryPush(const std::shared_ptr<FrontendEvent>& ev) {
    if (!ev) return false;
    std::unique_lock<std::mutex> lock(mtx_);
    if (buffer_.size() >= capacity_) return false;
    const auto ts = ev->timestamp();
    buffer_.emplace_back(ev, ts);
    last_timestamp_ = ts;
    lock.unlock();
    cv_.notify_all();
    return true;
}

void FrontendEventBuffer::pushFront(const std::shared_ptr<FrontendEvent>& ev) {
    if (!ev) return;

    std::unique_lock<std::mutex> lock(mtx_);
    // Returning an in-flight event must not evict a newer queued event. The
    // queue may exceed capacity by that returned event until it is drained.
    buffer_.emplace_front(ev, ev->timestamp());
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

    if (ev)
        ev->markConsumed(true);

    return ev;
}

std::shared_ptr<FrontendEvent> FrontendEventBuffer::waitAndPop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (!cv_.wait_for(lock, timeout, [&] { return !buffer_.empty(); }))
        return nullptr;

    auto ev = buffer_.front().first;
    buffer_.pop_front();

    if (ev)
        ev->markConsumed(true);

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

void FrontendEventBuffer::pruneUpTo(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lock(mtx_);
    while (!buffer_.empty() && buffer_.front().second <= t) {
        if (auto& ev = buffer_.front().first) {
            ev->markConsumed(true);
        }
        buffer_.pop_front();
    }
}
