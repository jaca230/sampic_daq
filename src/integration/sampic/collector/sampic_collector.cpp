#include "integration/sampic/collector/sampic_collector.h"
#include "integration/sampic/collector/modes/sampic_collector_mode_default.h"
#include "integration/sampic/collector/modes/sampic_collector_mode_example.h"

SampicCollector::SampicCollector(const SampicCollectorConfig& cfg,
                                 CrateInfoStruct& info,
                                 CrateParamStruct& params,
                                 void* eventBuffer,
                                 ML_Frame* mlFrames)
    : cfg_(cfg),
      info_(info),
      params_(params),
      eventBuffer_(eventBuffer),
      mlFrames_(mlFrames)
{
    buildMode();
    spdlog::info("SAMPIC Collector initialized (mode={}, buffer_size={})",
                 static_cast<int>(cfg_.mode), cfg_.buffer_size);
}

SampicCollector::~SampicCollector() {
    stop();
}

void SampicCollector::buildMode() {
    buffer_ = std::make_unique<SampicEventBuffer>(cfg_.buffer_size);

    switch (cfg_.mode) {
        case SampicCollectorModeType::DEFAULT:
            mode_ = std::make_unique<SampicCollectorModeDefault>(
                *buffer_, info_, params_, eventBuffer_, mlFrames_, cfg_);
            break;
        case SampicCollectorModeType::EXAMPLE:
            mode_ = std::make_unique<SampicCollectorModeExample>(
                *buffer_, info_, params_, eventBuffer_, mlFrames_, cfg_);
            break;
        default:
            throw std::runtime_error("Unsupported SampicCollectorModeType");
    }
}

void SampicCollector::setConfig(const SampicCollectorConfig& cfg) {
    cfg_ = cfg;
}

int SampicCollector::applySettings() {
    const bool was_running = running_;
    if (was_running) stop();

    try {
        buildMode();
        spdlog::info("SAMPIC Collector reconfigured (mode={}, buffer_size={})",
                     static_cast<int>(cfg_.mode), cfg_.buffer_size);

        if (was_running) start();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("SAMPIC Collector applySettings() failed: {}", e.what());
        return -1;
    }
}

void SampicCollector::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&SampicCollector::run, this);
}

void SampicCollector::stop() {
    if (running_) {
        running_ = false;
        if (worker_.joinable())
            worker_.join();
    }
}

void SampicCollector::run() {
    spdlog::info("SAMPIC Collector started (mode={})", static_cast<int>(cfg_.mode));

    while (running_) {
        bool ok = mode_->collect();
        if (!ok)
            spdlog::warn("SAMPIC Collector: collect() returned false");

        if (cfg_.sleep_time_us > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(cfg_.sleep_time_us));
    }

    spdlog::info("SAMPIC Collector stopped");
}
