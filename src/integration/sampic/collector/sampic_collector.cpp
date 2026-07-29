#include "integration/sampic/collector/sampic_collector.h"
SampicCollector::SampicCollector(const SampicCollectorConfig& cfg,
                                 CrateInfoStruct& info,
                                 CrateParamStruct& params,
                                 void* eventBuffer,
                                 ML_Frame* mlFrames,
                                 const ConfigStore& store,
                                 std::string modes_root)
    : cfg_(cfg),
      info_(info),
      params_(params),
      eventBuffer_(eventBuffer),
      mlFrames_(mlFrames),
      modes_root_(std::move(modes_root))
{
    buildMode(store);
    spdlog::info("SAMPIC Collector initialized (mode={}, buffer_size={})",
                 cfg_.mode, cfg_.buffer_size);
}

SampicCollector::~SampicCollector() {
    stop();
}

void SampicCollector::initializeOdb(ConfigStore& store, const std::string& modes_root) {
    SampicCollectorModeRegistry::catalog().initializeOdb(store, modes_root);
}

void SampicCollector::buildMode(const ConfigStore& store) {
    buffer_ = std::make_unique<SampicEventBuffer>(cfg_.buffer_size);

    SampicCollectorModeContext context{*buffer_, info_, params_, eventBuffer_, mlFrames_};
    mode_ = SampicCollectorModeRegistry::catalog().create(
        cfg_.mode, context, store, modes_root_);
}

void SampicCollector::setConfig(const SampicCollectorConfig& cfg) {
    cfg_ = cfg;
}

int SampicCollector::applySettings(const ConfigStore& store) {
    const bool was_running = worker_.running();
    if (was_running) stop();

    try {
        buildMode(store);
        spdlog::info("SAMPIC Collector reconfigured (mode={}, buffer_size={})",
                     cfg_.mode, cfg_.buffer_size);

        if (was_running) start();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("SAMPIC Collector applySettings() failed: {}", e.what());
        return -1;
    }
}

void SampicCollector::start() {
    worker_.start(
        [this] { return mode_->collect(); },
        std::chrono::microseconds(cfg_.sleep_time_us),
        [this] {
            spdlog::info("SAMPIC Collector started (mode={})", cfg_.mode);
        },
        [] { spdlog::info("SAMPIC Collector stopped"); });
}

void SampicCollector::stop() {
    worker_.stop();
}
