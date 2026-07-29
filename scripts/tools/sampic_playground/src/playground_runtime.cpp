#include "playground_runtime.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <rfl/json.hpp>

PlaygroundRuntime::PlaygroundRuntime(const SampicCollectorConfig& sampic_cfg,
                                     const SampicCollectorModeSimulatorConfig& simulator_cfg,
                                     const FrontendEventCollectorConfig& frontend_cfg,
                                     const FrontendCollectorModeDefaultConfig& frontend_mode_cfg)
    : sampic_cfg_(sampic_cfg),
      frontend_cfg_(frontend_cfg)
{
    SampicCollector::initializeOdb(store_, "/sampic/modes");
    FrontendEventCollector::initializeOdb(store_, "/frontend/modes");
    store_.writeJson("/sampic/modes/simulator",
                     ConfigStore::Json::parse(rfl::json::write(simulator_cfg)));
    store_.writeJson("/frontend/modes/default",
                     ConfigStore::Json::parse(rfl::json::write(frontend_mode_cfg)));

    // Create SAMPIC collector (uses simulator mode)
    sampic_collector_ = std::make_unique<SampicCollector>(
        sampic_cfg_, crate_info_, crate_params_, nullptr, nullptr,
        store_, "/sampic/modes");

    // Create Frontend collector
    frontend_collector_ = std::make_unique<FrontendEventCollector>(
        sampic_collector_->buffer(), frontend_cfg_, store_, "/frontend/modes");

    // Create fake MIDAS logger
    midas_logger_ = std::make_unique<FakeMidasLogger>(
        frontend_collector_->buffer());

    spdlog::info("Playground runtime initialized");
}

PlaygroundRuntime::~PlaygroundRuntime() {
    stop();
}

void PlaygroundRuntime::start() {
    spdlog::info("Starting pipeline...");
    sampic_collector_->start();
    frontend_collector_->start();
    midas_logger_->start();
    spdlog::info("All threads started");
}

void PlaygroundRuntime::stop() {
    spdlog::info("Stopping pipeline...");
    midas_logger_->stop();
    frontend_collector_->stop();
    sampic_collector_->stop();
    spdlog::info("All threads stopped");
}

void PlaygroundRuntime::run(std::chrono::seconds duration) {
    start();

    spdlog::info("Running for {} seconds...", duration.count());

    // Report every second
    auto start_time = std::chrono::steady_clock::now();
    auto last_report = start_time;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);

        // Report stats every second
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_report).count() >= 1000) {
            spdlog::info("Rate: {:.1f} kHz, {:.1f} MB/s",
                        getEventRate() / 1000.0,
                        getDataRate());
            last_report = now;
        }

        if (elapsed >= duration) {
            break;
        }
    }

    stop();
    printStatistics();
}

double PlaygroundRuntime::getEventRate() const {
    return midas_logger_->eventRate();
}

double PlaygroundRuntime::getDataRate() const {
    return midas_logger_->dataRate();
}

void PlaygroundRuntime::printStatistics() {
    spdlog::info("=== Final Statistics ===");
    spdlog::info("Total events logged: {}", midas_logger_->eventsLogged());
    spdlog::info("Total bytes logged:  {} ({:.2f} MB)",
                 midas_logger_->bytesLogged(),
                 midas_logger_->bytesLogged() / (1024.0 * 1024.0));
    spdlog::info("Event rate:          {:.1f} kHz", getEventRate() / 1000.0);
    spdlog::info("Data rate:           {:.2f} MB/s", getDataRate());
    spdlog::info("========================");
}
