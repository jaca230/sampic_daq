#ifndef SAMPIC_DAQ_CORE_THREADING_WORKER_THREAD_CONFIG_H
#define SAMPIC_DAQ_CORE_THREADING_WORKER_THREAD_CONFIG_H

#include <string>

struct WorkerThreadConfig {
    std::string name;
    int core_hint = -1;
    int realtime_priority = 0;
};

#endif
