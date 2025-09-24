#ifndef SAMPIC_COLLECTOR_H
#define SAMPIC_COLLECTOR_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/collector/modes/sampic_collector_mode.h"

#include <thread>
#include <atomic>
#include <memory>
#include <spdlog/spdlog.h>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

/// Collector thread for reading events from SAMPIC hardware and filling the buffer.
class SampicCollector {
public:
    SampicCollector(const SampicCollectorConfig& cfg,
                    CrateInfoStruct& info,
                    CrateParamStruct& params,
                    void* eventBuffer,
                    ML_Frame* mlFrames);

    ~SampicCollector();

    void start();
    void stop();
    bool running() const { return running_; }

    // Buffer access
    SampicEventBuffer& buffer() { return *buffer_; }
    const SampicEventBuffer& buffer() const { return *buffer_; }

private:
    void run();

    SampicCollectorConfig cfg_;
    std::unique_ptr<SampicEventBuffer> buffer_;
    EventStruct event_{};  ///< reused event struct

    std::unique_ptr<SampicCollectorMode> mode_; ///< polymorphic mode

    std::thread worker_;
    std::atomic<bool> running_{false};
};

#endif // SAMPIC_COLLECTOR_H
