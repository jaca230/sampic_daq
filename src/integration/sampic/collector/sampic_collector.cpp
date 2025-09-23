#include "integration/sampic/collector/sampic_collector.h"
#include "integration/sampic/collector/modes/sampic_collector_mode_default.h"
#include "integration/sampic/collector/modes/sampic_collector_mode_example.h"

SampicCollector::SampicCollector(const SampicCollectorConfig& cfg,
                                 SampicEventBuffer& buffer,
                                 CrateInfoStruct& info,
                                 CrateParamStruct& params,
                                 void* eventBuffer,
                                 ML_Frame* mlFrames)
    : cfg_(cfg), buffer_(buffer)
{
    switch (cfg.mode) {
        case SampicCollectorMode::DEFAULT:
            mode_ = std::make_unique<SampicCollectorModeDefault>(info, params, eventBuffer, mlFrames, cfg);
            break;
        case SampicCollectorMode::EXAMPLE:
            mode_ = std::make_unique<SampicCollectorModeExample>(info, params, eventBuffer, mlFrames, cfg);
            break;
        default:
            throw std::runtime_error("Unsupported SampicCollectorMode");
    }
}

SampicCollector::~SampicCollector() {
    stop();
}

void SampicCollector::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&SampicCollector::run, this);
}

void SampicCollector::stop() {
    if (running_) {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }
}

void SampicCollector::run() {
    spdlog::info("SAMPIC Collector started in mode {}",
                 cfg_.mode == SampicCollectorMode::DEFAULT ? "DEFAULT" : "EXAMPLE");

    while (running_) {
        auto start_total = std::chrono::steady_clock::now();

        SampicEventTiming timing{};
        int hits = mode_->readEvent(event_, timing);

        auto end_total = std::chrono::steady_clock::now();
        timing.total_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_total - start_total);

        if (hits > 0) {
            TimestampedSampicEvent tse{event_, std::chrono::steady_clock::now(), timing};
            buffer_.push(tse);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(cfg_.sleep_time_us));
    }

    spdlog::info("SAMPIC Collector stopped");
}
