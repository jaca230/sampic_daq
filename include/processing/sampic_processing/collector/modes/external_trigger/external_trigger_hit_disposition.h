#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_DISPOSITION_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_HIT_DISPOSITION_H

enum class ExternalTriggerHitDisposition {
    Assigned,
    NoTriggerRecords,
    OutsideAssociationWindow,
};

#endif
