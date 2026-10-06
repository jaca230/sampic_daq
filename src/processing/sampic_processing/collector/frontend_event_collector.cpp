#include "processing/sampic_processing/collector/frontend_event_collector.h"
FrontendEventCollector::FrontendEventCollector(
    SampicEventBuffer& sampic_buffer,
    const FrontendEventCollectorConfig& cfg,
    const ConfigStore& store,
    std::string modes_root)
    : sampic_buffer_(sampic_buffer),
      cfg_(cfg),
      modes_root_(std::move(modes_root))
{
    diagnostics_ = std::make_unique<frontend::collector::FrontendDiagnostics>(cfg_.diagnostics);
    buildMode(store);
    spdlog::info("FrontendEventCollector initialized (mode={}, buffer_size={})",
                 cfg_.mode, cfg_.buffer_size);
}

FrontendEventCollector::~FrontendEventCollector() {
    stop();
}

void FrontendEventCollector::initializeOdb(ConfigStore& store, const std::string& modes_root) {
    FrontendCollectorModeRegistry::catalog().initializeOdb(store, modes_root);
}

void FrontendEventCollector::buildMode(const ConfigStore& store) {
    buffer_ = std::make_unique<FrontendEventBuffer>(cfg_.buffer_size);
    diagnostics_ = std::make_unique<frontend::collector::FrontendDiagnostics>(cfg_.diagnostics);

    FrontendCollectorModeContext context{
        sampic_buffer_, *buffer_, cfg_.diagnostics, *diagnostics_};
    mode_ = FrontendCollectorModeRegistry::catalog().create(
        cfg_.mode, context, store, modes_root_);
}

void FrontendEventCollector::setConfig(const FrontendEventCollectorConfig& cfg) {
    cfg_ = cfg;
}

int FrontendEventCollector::applySettings(const ConfigStore& store) {
    const bool was_running = worker_.running();
    if (was_running) stop();

    try {
        buildMode(store);
        spdlog::info("FrontendEventCollector reconfigured (mode={}, buffer_size={})",
                     cfg_.mode, cfg_.buffer_size);

        if (was_running) start();
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("FrontendEventCollector applySettings() failed: {}", e.what());
        return -1;
    }
}

void FrontendEventCollector::start() {
    worker_.start(
        [this] { return mode_->collect(); },
        std::chrono::microseconds(cfg_.sleep_time_us),
        [this] {
            spdlog::info(
                "FrontendEventCollector started (mode={})", cfg_.mode);
        },
        [] { spdlog::info("FrontendEventCollector stopped"); });
}

void FrontendEventCollector::stop() {
    worker_.stop();
}

bool FrontendEventCollector::drain(std::size_t max_cycles) {
    stop();

    std::size_t cycles = 0;
    while (!sampic_buffer_.empty()) {
        if (cycles++ >= max_cycles) {
            spdlog::error(
                "FrontendEventCollector drain exceeded {} processing cycles "
                "with {} SAMPIC event(s) remaining",
                max_cycles, sampic_buffer_.size());
            return false;
        }

        const auto before = sampic_buffer_.size();
        if (!mode_->collect()) {
            spdlog::error(
                "FrontendEventCollector mode failed while draining");
            return false;
        }
        const auto after = sampic_buffer_.size();
        if (after >= before) {
            spdlog::error(
                "FrontendEventCollector drain made no progress "
                "({} SAMPIC event(s) remaining)",
                after);
            return false;
        }
    }

    if (!mode_->flush()) {
        spdlog::error(
            "FrontendEventCollector mode failed to flush pending state");
        return false;
    }

    spdlog::info(
        "FrontendEventCollector drain complete after {} processing cycle(s); "
        "{} frontend event(s) ready for MIDAS",
        cycles, buffer_->size());
    return true;
}
