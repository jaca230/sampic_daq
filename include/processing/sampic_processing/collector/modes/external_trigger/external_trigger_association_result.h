#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_ASSOCIATION_RESULT_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_ASSOCIATION_RESULT_H

#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_association_stats.h"
#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_hit_decision.h"

#include <cstdint>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

struct ExternalTriggerAssociationResult {
    std::vector<std::vector<const HitStruct*>> hits_by_trigger;
    std::vector<std::uint32_t> ambiguous_hits_by_trigger;
    std::vector<ExternalTriggerHitDecision> hit_decisions;
    ExternalTriggerAssociationStats stats;
};

#endif
