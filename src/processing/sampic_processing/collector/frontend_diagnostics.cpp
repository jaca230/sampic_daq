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
    const auto warn_threshold = cfg_.buffer_warning_threshold;
    if (warn_threshold > 0 && buffer_size >= warn_threshold) {
        spdlog::warn("Frontend buffer occupancy high: {} events queued (threshold {})",
                     buffer_size, warn_threshold);
    }

    const auto interval = std::chrono::milliseconds(cfg_.log_interval_ms);
    if (interval.count() == 0)
        return;

    if (now - last_log_time_ < interval)
        return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    const double seconds = elapsed.count() > 0 ? static_cast<double>(elapsed.count()) : 1.0;
    const double rate = produced_events_ / seconds;
    const double hit_rate = produced_hits_ / seconds;
    const double backpressure = produced_events_ > 0
                                    ? static_cast<double>(produced_events_ - consumed_events_) /
                                          produced_events_
                                    : 0.0;

    spdlog::info("Frontend diagnostics: produced={} consumed={} backlog={} buffer={} rate={:.2f} evt/s hits={:.2f} hit/s",
                 produced_events_, consumed_events_,
                 produced_events_ - consumed_events_, buffer_size,
                 rate, hit_rate);

    last_log_time_ = now;
}

}  // namespace frontend::collector
