#include "fake_midas_logger.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <vector>

FakeMidasLogger::FakeMidasLogger(FrontendEventBuffer& buffer)
    : buffer_(buffer) {}

FakeMidasLogger::~FakeMidasLogger() {
    stop();
}

void FakeMidasLogger::start() {
    if (running_) return;
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    worker_ = std::thread(&FakeMidasLogger::run, this);
}

void FakeMidasLogger::stop() {
    if (running_) {
        running_ = false;
        if (worker_.joinable())
            worker_.join();
    }
}

double FakeMidasLogger::eventRate() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - start_time_).count();
    if (elapsed < 0.001) return 0.0;
    return events_logged_.load() / elapsed;
}

double FakeMidasLogger::dataRate() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - start_time_).count();
    if (elapsed < 0.001) return 0.0;
    double mb = bytes_logged_.load() / (1024.0 * 1024.0);
    return mb / elapsed;
}

size_t FakeMidasLogger::calculateEventSize(const std::shared_ptr<FrontendEvent>& event) {
    size_t total = 0;
    const auto banks = event->banks();
    for (auto* bank : banks) {
        if (!bank) continue;

        // Bank header overhead (name + size field, ~16 bytes per bank in MIDAS)
        total += 16;

        // Bank data
        if (auto* data_bank = dynamic_cast<FrontendEventBankData*>(bank)) {
            const auto& slices = data_bank->slices();
            for (const auto& slice : slices) {
                total += slice.second;
            }
        } else {
            total += bank->size();
        }
    }
    // Event header overhead (~32 bytes in MIDAS)
    total += 32;
    return total;
}

void FakeMidasLogger::run() {
    spdlog::debug("FakeMidasLogger thread started");

    // Allocate a buffer for copying event data (like MIDAS does)
    std::vector<uint8_t> event_buffer;
    event_buffer.reserve(8 * 1024 * 1024);  // 8 MB buffer

    while (running_) {
        auto event = buffer_.waitAndPop(std::chrono::milliseconds(100));
        if (!event) {
            continue;
        }

        // Mimic MIDAS: serialize the event into a buffer via memcpy
        event_buffer.clear();

        // Event header (32 bytes like MIDAS)
        event_buffer.resize(32);

        const auto banks = event->banks();
        for (auto* bank : banks) {
            if (!bank) continue;

            // Bank header (16 bytes: name + size)
            size_t bank_header_pos = event_buffer.size();
            event_buffer.resize(event_buffer.size() + 16);

            // Bank data - memcpy like MIDAS does
            if (auto* data_bank = dynamic_cast<FrontendEventBankData*>(bank)) {
                const auto& slices = data_bank->slices();
                for (const auto& slice : slices) {
                    size_t old_size = event_buffer.size();
                    event_buffer.resize(old_size + slice.second);
                    std::memcpy(event_buffer.data() + old_size, slice.first, slice.second);
                }
            } else {
                size_t old_size = event_buffer.size();
                size_t bank_size = bank->size();
                event_buffer.resize(old_size + bank_size);
                std::memcpy(event_buffer.data() + old_size, bank->data(), bank_size);
            }
        }

        // Count the event
        size_t total_bytes = event_buffer.size();
        events_logged_.fetch_add(1, std::memory_order_relaxed);
        bytes_logged_.fetch_add(total_bytes, std::memory_order_relaxed);
    }

    spdlog::debug("FakeMidasLogger thread stopped");
}
