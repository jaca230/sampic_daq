#include "core/threading/polling_worker.h"

#include <spdlog/spdlog.h>

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
    thread_ = std::thread(
        &PollingWorker::run, this, std::move(work), interval,
        std::move(on_started), std::move(on_stopped));
}

void PollingWorker::stop() {
    if (!running_.exchange(false)) return;
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

    while (running_) {
        if (!work()) {
            spdlog::warn("{}: polling operation returned false", config_.name);
        }
        if (interval.count() > 0) std::this_thread::sleep_for(interval);
    }

    if (on_stopped) on_stopped();
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
