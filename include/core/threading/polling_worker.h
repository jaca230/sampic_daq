#ifndef SAMPIC_DAQ_CORE_THREADING_POLLING_WORKER_H
#define SAMPIC_DAQ_CORE_THREADING_POLLING_WORKER_H

#include "core/threading/worker_thread_config.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

class PollingWorker {
public:
    using Work = std::function<bool()>;
    using LifecycleCallback = std::function<void()>;

    explicit PollingWorker(WorkerThreadConfig config);
    ~PollingWorker();

    PollingWorker(const PollingWorker&) = delete;
    PollingWorker& operator=(const PollingWorker&) = delete;

    void start(Work work,
               std::chrono::microseconds interval,
               LifecycleCallback on_started = {},
               LifecycleCallback on_stopped = {});
    void stop();
    bool running() const;

private:
    void configureCurrentThread() const;
    void run(Work work,
             std::chrono::microseconds interval,
             LifecycleCallback on_started,
             LifecycleCallback on_stopped);

    WorkerThreadConfig config_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

#endif
