#include "processing/sampic_processing/collector/frontend_diagnostics.h"

#include <spdlog/spdlog.h>

namespace frontend::collector {

FrontendDiagnostics::FrontendDiagnostics(const FrontendCollectorDiagnosticsConfig& cfg)
    : cfg_(cfg) {
    const auto now = std::chrono::steady_clock::now();
    start_time_ = now;
    last_log_time_ = now;
}

void FrontendDiagnostics::produced(size_t events, size_t hits, size_t buffer_size) {
    if (!cfg_.enabled)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    produced_events_ += events;
    produced_hits_ += hits;
    maybe_log_locked(std::chrono::steady_clock::now(), buffer_size);
}

void FrontendDiagnostics::consumed(size_t events, size_t buffer_size) {
    if (!cfg_.enabled)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    consumed_events_ += events;
    maybe_log_locked(std::chrono::steady_clock::now(), buffer_size);
}

void FrontendDiagnostics::maybe_log_locked(std::chrono::steady_clock::time_point now,
                                           size_t buffer_size) {
    // Diagnostic logging removed
}

}  // namespace frontend::collector
