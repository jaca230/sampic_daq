#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode_default.h"

FrontendEventCollector::FrontendEventCollector(
    SampicEventBuffer& sampic_buffer,
    const FrontendEventCollectorConfig& cfg)
    : sampic_buffer_(sampic_buffer),
      cfg_(cfg)
{
    buildMode();
    spdlog::info("FrontendEventCollector initialized (mode={}, buffer_size={})",
                 static_cast<int>(cfg_.mode), cfg_.buffer_size);
}

FrontendEventCollector::~FrontendEventCollector() {
    stop();
}

void FrontendEventCollector::buildMode() {
    buffer_ = std::make_unique<FrontendEventBuffer>(cfg_.buffer_size);

    switch (cfg_.mode) {
        case FrontendCollectorModeType::DEFAULT:
            mode_ = std::make_unique<FrontendCollectorModeDefault>(
                sampic_buffer_, *buffer_, cfg_);
            break;
        default:
            throw std::runtime_error("Unsupported FrontendCollectorModeType");
    }
}

void FrontendEventCollector::setConfig(const FrontendEventCollectorConfig& cfg) {
    cfg_ = cfg;
}

int FrontendEventCollector::applySettings() {
    const bool was_running = running_;
    if (was_running) stop();

    try {
        buildMode();
        spdlog::info("FrontendEventCollector reconfigured (mode={}, buffer_size={})",
                     static_cast<int>(cfg_.mode), cfg_.buffer_size);

        if (was_running) start();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("FrontendEventCollector applySettings() failed: {}", e.what());
        return -1;
    }
}

void FrontendEventCollector::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&FrontendEventCollector::run, this);
}

void FrontendEventCollector::stop() {
    if (running_) {
        running_ = false;
        if (worker_.joinable())
            worker_.join();
    }
}

void FrontendEventCollector::run() {
    spdlog::info("FrontendEventCollector started (mode={})", static_cast<int>(cfg_.mode));

    while (running_) {
        bool ok = mode_->collect();
        if (!ok)
            spdlog::warn("FrontendEventCollector: collect() returned false");

        if (cfg_.sleep_time_us > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(cfg_.sleep_time_us));
    }

    spdlog::info("FrontendEventCollector stopped");
}
