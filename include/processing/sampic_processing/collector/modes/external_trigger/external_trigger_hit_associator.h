#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_ASSOCIATOR_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_ASSOCIATOR_H

#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_association_result.h"

class ExternalTriggerHitAssociator {
public:
    ExternalTriggerHitAssociator(
        double hit_time_offset_ns,
        double pre_window_ns,
        double post_window_ns,
        double sampling_frequency_mhz);

    ExternalTriggerAssociationResult associate(const EventStruct& event) const;

private:
    double hit_time_offset_ns_;
    double pre_window_ns_;
    double post_window_ns_;
    double sample_period_ns_;
};

#endif
