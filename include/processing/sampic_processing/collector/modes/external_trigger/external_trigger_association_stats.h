#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_ASSOCIATION_STATS_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_ASSOCIATION_STATS_H

#include <cstddef>

struct ExternalTriggerAssociationStats {
    std::size_t raw_hits = 0;
    std::size_t trigger_records = 0;
    std::size_t assigned_hits = 0;
    std::size_t hits_without_trigger_records = 0;
    std::size_t hits_outside_window = 0;
    std::size_t hits_inside_multiple_windows = 0;
    std::size_t triggers_with_hits = 0;
    std::size_t triggers_without_hits = 0;
};

#endif
