#ifndef FAKE_MIDAS_LOGGER_H
#define FAKE_MIDAS_LOGGER_H

#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include <thread>
#include <atomic>
#include <chrono>

/**
 * @brief Simulates the MIDAS logging thread
 *
 * Consumes FrontendEvents from the buffer and "logs" them (counts bytes/events).
 * This mimics what the real MIDAS frontend does when writing to .mid files.
 */
class FakeMidasLogger {
public:
    explicit FakeMidasLogger(FrontendEventBuffer& buffer);
    ~FakeMidasLogger();

    void start();
    void stop();
    bool running() const { return running_; }

    // Statistics
    uint64_t eventsLogged() const { return events_logged_; }
    uint64_t bytesLogged() const { return bytes_logged_; }
    double eventRate() const;  // Events/sec
    double dataRate() const;   // MB/sec

private:
    void run();
    size_t calculateEventSize(const std::shared_ptr<FrontendEvent>& event);

    FrontendEventBuffer& buffer_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    std::atomic<uint64_t> events_logged_{0};
    std::atomic<uint64_t> bytes_logged_{0};
    std::chrono::steady_clock::time_point start_time_;
};

#endif // FAKE_MIDAS_LOGGER_H
