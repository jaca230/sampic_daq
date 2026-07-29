#ifndef FRONTEND_EVENT_COLLECTOR_H
#define FRONTEND_EVENT_COLLECTOR_H

#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/frontend_diagnostics.h"
#include "integration/sampic/collector/sampic_event_buffer.h"
#include "core/threading/polling_worker.h"

#include <memory>
#include <cstddef>
#include <spdlog/spdlog.h>
#include "core/config/config_store.h"

/**
 * @brief Threaded manager that runs a FrontendCollectorMode.
 * The mode handles fetching from the Sampic buffer and producing frontend events.
 */
class FrontendEventCollector {
public:
    FrontendEventCollector(SampicEventBuffer& sampic_buffer,
                           const FrontendEventCollectorConfig& cfg,
                           const ConfigStore& store,
                           std::string modes_root);
    ~FrontendEventCollector();

    void start();
    void stop();
    bool drain(std::size_t max_cycles = 10'000);
    bool running() const { return worker_.running(); }

    void setConfig(const FrontendEventCollectorConfig& cfg);
    int  applySettings(const ConfigStore& store);
    static void initializeOdb(ConfigStore& store, const std::string& modes_root);

    const FrontendEventCollectorConfig& config() const { return cfg_; }
    frontend::collector::FrontendDiagnostics& diagnostics() { return *diagnostics_; }
    const frontend::collector::FrontendDiagnostics& diagnostics() const { return *diagnostics_; }

    FrontendEventBuffer& buffer() { return *buffer_; }
    const FrontendEventBuffer& buffer() const { return *buffer_; }

private:
    void buildMode(const ConfigStore& store);

    SampicEventBuffer& sampic_buffer_;
    FrontendEventCollectorConfig cfg_;
    std::string modes_root_;
    std::unique_ptr<FrontendEventBuffer> buffer_;
    std::unique_ptr<FrontendCollectorMode> mode_;
    std::unique_ptr<frontend::collector::FrontendDiagnostics> diagnostics_;

    PollingWorker worker_{{"fe_collector", 1, 10}};
};

#endif // FRONTEND_EVENT_COLLECTOR_H
