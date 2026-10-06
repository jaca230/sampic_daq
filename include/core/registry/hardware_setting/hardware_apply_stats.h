#ifndef SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_APPLY_STATS_H
#define SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_APPLY_STATS_H

#include <chrono>
#include <cstddef>

struct HardwareApplyStats {
    std::size_t checked = 0;
    std::size_t changed = 0;
    std::chrono::milliseconds elapsed{0};
};

#endif
