#include "core/threading/polling_worker.h"

#include <spdlog/spdlog.h>
#include <csignal>
#include <cstdio>
#include <stdexcept>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

PollingWorker::PollingWorker(WorkerThreadConfig config)
    : config_(std::move(config)) {}

PollingWorker::~PollingWorker() {
    stop();
}

void PollingWorker::start(Work work,
                          std::chrono::microseconds interval,
                          LifecycleCallback on_started,
                          LifecycleCallback on_stopped) {
    if (running_.exchange(true)) return;
    if (thread_.joinable()) thread_.join();
    thread_ = std::thread(
        &PollingWorker::run, this, std::move(work), interval,
        std::move(on_started), std::move(on_stopped));
}

void PollingWorker::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

bool PollingWorker::running() const {
    return running_.load();
}

void PollingWorker::run(Work work,
                        std::chrono::microseconds interval,
                        LifecycleCallback on_started,
                        LifecycleCallback on_stopped) {
    configureCurrentThread();
    if (on_started) on_started();

    bool overflow = false;
    try {
        while (running_) {
            if (!work()) {
                spdlog::warn("{}: polling operation returned false", config_.name);
            }
            if (interval.count() > 0) std::this_thread::sleep_for(interval);
        }
    } catch (const std::overflow_error& error) {
        running_ = false;
        overflow = true;
        spdlog::critical("{}: {}; terminating frontend", config_.name, error.what());
        std::fprintf(stderr, "FATAL %s: %s; terminating frontend\n", config_.name.c_str(), error.what());
        std::fflush(stderr);
    }

    if (on_stopped) on_stopped();
    // MIDAS already handles SIGTERM on its main thread. The failing worker
    // has stopped immediately; its lifecycle callback stops hardware when it
    // owns acquisition. Main-thread frontend_exit cleans up the other workers.
    if (overflow) std::raise(SIGTERM);
}

void PollingWorker::configureCurrentThread() const {
#ifdef __linux__
    const pthread_t handle = pthread_self();
    if (!config_.name.empty()) {
        pthread_setname_np(handle, config_.name.substr(0, 15).c_str());
    }

    if (config_.core_hint >= 0) {
        const long core_count = sysconf(_SC_NPROCESSORS_ONLN);
        if (core_count > 0) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(config_.core_hint % core_count, &cpuset);
            pthread_setaffinity_np(handle, sizeof(cpuset), &cpuset);
        }
    }

    sched_param scheduling{};
    scheduling.sched_priority = config_.realtime_priority;
    if (config_.realtime_priority <= 0 ||
        pthread_setschedparam(handle, SCHED_FIFO, &scheduling) != 0) {
        scheduling.sched_priority = 0;
        pthread_setschedparam(handle, SCHED_OTHER, &scheduling);
    }
#endif
}
