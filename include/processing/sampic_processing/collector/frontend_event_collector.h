#ifndef FRONTEND_EVENT_COLLECTOR_H
#define FRONTEND_EVENT_COLLECTOR_H

#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "integration/sampic/collector/sampic_event_buffer.h"

#include <thread>
#include <atomic>
#include <memory>
#include <spdlog/spdlog.h>

/**
 * @brief Threaded manager that runs a FrontendCollectorMode.
 * The mode handles fetching from the Sampic buffer and producing frontend events.
 */
class FrontendEventCollector {
public:
    FrontendEventCollector(SampicEventBuffer& sampic_buffer,
                           const FrontendEventCollectorConfig& cfg);
    ~FrontendEventCollector();

    void start();
    void stop();
    bool running() const { return running_; }

    void setConfig(const FrontendEventCollectorConfig& cfg);
    int  applySettings();

    const FrontendEventCollectorConfig& config() const { return cfg_; }

    FrontendEventBuffer& buffer() { return *buffer_; }
    const FrontendEventBuffer& buffer() const { return *buffer_; }

private:
    void run();
    void buildMode(); ///< internal factory for collector mode

    SampicEventBuffer& sampic_buffer_;
    FrontendEventCollectorConfig cfg_;
    std::unique_ptr<FrontendEventBuffer> buffer_;
    std::unique_ptr<FrontendCollectorMode> mode_;

    std::thread worker_;
    std::atomic<bool> running_{false};
};

#endif // FRONTEND_EVENT_COLLECTOR_H
