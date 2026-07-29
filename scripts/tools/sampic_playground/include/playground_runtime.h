#ifndef PLAYGROUND_RUNTIME_H
#define PLAYGROUND_RUNTIME_H

#include "integration/sampic/collector/sampic_collector.h"
#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "fake_midas_logger.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "integration/sampic/collector/modes/simulator/simulator_config.h"
#include "processing/sampic_processing/collector/modes/default/default_config.h"
#include "core/config/memory_config_store.h"
#include <memory>
#include <chrono>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/**
 * @brief Manages the full DAQ pipeline for benchmarking
 *
 * This class orchestrates the three-stage pipeline:
 * 1. SampicCollector (simulator mode) -> SampicEventBuffer
 * 2. FrontendEventCollector -> FrontendEventBuffer
 * 3. FakeMidasLogger (consumes and measures)
 */
class PlaygroundRuntime {
public:
    PlaygroundRuntime(const SampicCollectorConfig& sampic_cfg,
                      const SampicCollectorModeSimulatorConfig& simulator_cfg,
                      const FrontendEventCollectorConfig& frontend_cfg,
                      const FrontendCollectorModeDefaultConfig& frontend_mode_cfg);
    ~PlaygroundRuntime();

    void start();
    void stop();
    void run(std::chrono::seconds duration);

    // Statistics
    void printStatistics();
    double getEventRate() const;
    double getDataRate() const;

private:
    SampicCollectorConfig sampic_cfg_;
    FrontendEventCollectorConfig frontend_cfg_;
    MemoryConfigStore store_;

    // Hardware stubs (not used in simulator mode)
    CrateInfoStruct crate_info_{};
    CrateParamStruct crate_params_{};

    // Pipeline components
    std::unique_ptr<SampicCollector> sampic_collector_;
    std::unique_ptr<FrontendEventCollector> frontend_collector_;
    std::unique_ptr<FakeMidasLogger> midas_logger_;
};

#endif // PLAYGROUND_RUNTIME_H
