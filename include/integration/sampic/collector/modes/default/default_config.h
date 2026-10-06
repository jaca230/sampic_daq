#ifndef SAMPIC_COLLECTOR_MODE_DEFAULT_CONFIG_H
#define SAMPIC_COLLECTOR_MODE_DEFAULT_CONFIG_H

struct SampicCollectorModeDefaultConfig {
    int soft_trigger_prepare_interval = 100;
    int soft_trigger_max_loops = 10000;
    int soft_trigger_retry_sleep_us = 100;
};

#endif
