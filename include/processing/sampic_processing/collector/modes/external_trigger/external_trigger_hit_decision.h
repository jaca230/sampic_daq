#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_DECISION_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_DECISION_H

#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_hit_disposition.h"

#include <cstddef>
#include <optional>

struct ExternalTriggerHitDecision {
    std::size_t hit_index = 0;
    ExternalTriggerHitDisposition disposition =
        ExternalTriggerHitDisposition::NoTriggerRecords;
    std::optional<std::size_t> nearest_trigger_index;
    std::optional<std::size_t> assigned_trigger_index;
    std::size_t triggers_inside_window = 0;
    double hit_timestamp_ns = 0.0;
    double nearest_trigger_timestamp_ns = 0.0;
    double nearest_reference_timestamp_ns = 0.0;
    double hit_minus_reference_ns = 0.0;
};

#endif
