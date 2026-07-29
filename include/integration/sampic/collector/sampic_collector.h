#ifndef SAMPIC_COLLECTOR_H
#define SAMPIC_COLLECTOR_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/collector/modes/sampic_collector_mode.h"
#include "core/threading/polling_worker.h"

#include <memory>
#include <spdlog/spdlog.h>
#include "core/config/config_store.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/**
 * @brief Threaded collector that runs a chosen SAMPICCollectorMode.
 * The mode performs acquisition and pushes SampicEvent objects into the buffer.
 */
class SampicCollector {
public:
    SampicCollector(const SampicCollectorConfig& cfg,
                    CrateInfoStruct& info,
                    CrateParamStruct& params,
                    void* eventBuffer,
                    ML_Frame* mlFrames,
                    const ConfigStore& store,
                    std::string modes_root);
    ~SampicCollector();

    void start();
    void stop();
    bool running() const { return worker_.running(); }

    /** @brief Update configuration; does not rebuild collector. */
    void setConfig(const SampicCollectorConfig& cfg);

    /** @brief Rebuild mode + buffer with current configuration. */
    int applySettings(const ConfigStore& store);
    static void initializeOdb(ConfigStore& store, const std::string& modes_root);

    SampicEventBuffer& buffer() { return *buffer_; }
    const SampicEventBuffer& buffer() const { return *buffer_; }

private:
    void buildMode(const ConfigStore& store);

    SampicCollectorConfig cfg_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void* eventBuffer_;
    ML_Frame* mlFrames_;
    std::string modes_root_;

    std::unique_ptr<SampicEventBuffer> buffer_;
    std::unique_ptr<SampicCollectorMode> mode_;

    PollingWorker worker_{{"sampic_collector", 0, 12}};
};

#endif // SAMPIC_COLLECTOR_H
