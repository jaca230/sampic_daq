#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode_default.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode_external_trigger.h"
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace {
#ifdef __linux__
void configure_worker_thread(const char* name, int core_hint) {
    pthread_t handle = pthread_self();
    if (name) {
        pthread_setname_np(handle, name);
    }

    if (core_hint >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        const long core_count = sysconf(_SC_NPROCESSORS_ONLN);
        if (core_count > 0) {
            CPU_SET(core_hint % core_count, &cpuset);
            pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
        }
    }

    sched_param sch{};
    sch.sched_priority = 10;
    if (pthread_setschedparam(handle, SCHED_FIFO, &sch) != 0) {
        sch.sched_priority = 0;
        pthread_setschedparam(handle, SCHED_OTHER, &sch);
    }
}
#else
void configure_worker_thread(const char*, int) {}
#endif
} // namespace

FrontendEventCollector::FrontendEventCollector(
    SampicEventBuffer& sampic_buffer,
    const FrontendEventCollectorConfig& cfg)
    : sampic_buffer_(sampic_buffer),
      cfg_(cfg)
{
    diagnostics_ = std::make_unique<frontend::collector::FrontendDiagnostics>(cfg_.diagnostics);
    buildMode();
    spdlog::info("FrontendEventCollector initialized (mode={}, buffer_size={})",
                 static_cast<int>(cfg_.mode), cfg_.buffer_size);
}

FrontendEventCollector::~FrontendEventCollector() {
    stop();
}

void FrontendEventCollector::buildMode() {
    buffer_ = std::make_unique<FrontendEventBuffer>(cfg_.buffer_size);
    diagnostics_ = std::make_unique<frontend::collector::FrontendDiagnostics>(cfg_.diagnostics);

    switch (cfg_.mode) {
        case FrontendCollectorModeType::DEFAULT:
            mode_ = std::make_unique<FrontendCollectorModeDefault>(
                sampic_buffer_, *buffer_, cfg_, *diagnostics_);
            break;
        case FrontendCollectorModeType::EXTERNAL_TRIGGER:
            mode_ = std::make_unique<FrontendCollectorModeExternalTrigger>(
                sampic_buffer_, *buffer_, cfg_, *diagnostics_);
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
    configure_worker_thread("fe_collector", 1);
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
