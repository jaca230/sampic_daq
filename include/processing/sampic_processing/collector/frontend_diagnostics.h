#ifndef FRONTEND_DIAGNOSTICS_H
#define FRONTEND_DIAGNOSTICS_H

#include "processing/sampic_processing/config/frontend_event_collector_config.h"

#include <atomic>
#include <chrono>
#include <mutex>

namespace frontend::collector {

class FrontendDiagnostics {
public:
    explicit FrontendDiagnostics(const FrontendCollectorDiagnosticsConfig& cfg);

    void produced(size_t events, size_t hits, size_t buffer_size);
    void consumed(size_t events, size_t buffer_size);

    const FrontendCollectorDiagnosticsConfig& config() const { return cfg_; }

private:
    void maybe_log_locked(std::chrono::steady_clock::time_point now,
                          size_t buffer_size);

    FrontendCollectorDiagnosticsConfig cfg_{};
    std::chrono::steady_clock::time_point start_time_{};
    std::chrono::steady_clock::time_point last_log_time_{};

    uint64_t produced_events_ = 0;
    uint64_t produced_hits_ = 0;
    uint64_t consumed_events_ = 0;

    std::mutex mutex_;
};

}  // namespace frontend::collector

#endif  // FRONTEND_DIAGNOSTICS_H
