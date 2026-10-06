#ifndef SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_TOPOLOGY_H
#define SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_TOPOLOGY_H

struct HardwareTopology {
    int febs = 4;
    int chips_per_feb = 4;
    int channels_per_chip = 16;
};

#endif
