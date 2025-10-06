#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include <spdlog/spdlog.h>

FrontendEventBuffer::FrontendEventBuffer(size_t capacity)
    : capacity_(capacity) {}

void FrontendEventBuffer::push(const std::shared_ptr<FrontendEvent>& ev) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (buffer_.size() >= capacity_) buffer_.pop_front();
    auto now = std::chrono::steady_clock::now();
    buffer_.emplace_back(ev, now);
    last_timestamp_ = now;
    cv_.notify_all();
}

std::optional<std::shared_ptr<FrontendEvent>> FrontendEventBuffer::pop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (buffer_.empty()) return std::nullopt;
    auto ev = buffer_.front().first;
    buffer_.pop_front();
    return ev;
}

std::optional<std::shared_ptr<FrontendEvent>> FrontendEventBuffer::latest() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (buffer_.empty()) return std::nullopt;
    return buffer_.back().first;
}

std::vector<std::shared_ptr<FrontendEvent>> FrontendEventBuffer::getSince(std::chrono::steady_clock::time_point t) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::shared_ptr<FrontendEvent>> result;
    for (auto& [ev, ts] : buffer_) {
        if (ts > t) result.push_back(ev);
    }
    return result;
}

bool FrontendEventBuffer::hasNewSince(std::chrono::steady_clock::time_point t) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return last_timestamp_ > t;
}

bool FrontendEventBuffer::waitForNew(std::chrono::steady_clock::time_point t,
                                     std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, timeout, [&]{ return last_timestamp_ > t; });
}

size_t FrontendEventBuffer::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return buffer_.size();
}

bool FrontendEventBuffer::empty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return buffer_.empty();
}
