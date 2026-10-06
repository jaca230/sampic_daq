#ifndef SAMPIC_DAQ_EXTERNAL_TRIGGER_COUNTER_SETTING_H
#define SAMPIC_DAQ_EXTERNAL_TRIGGER_COUNTER_SETTING_H

struct ExternalTriggerCounterSetting {
    bool enabled = false;
    bool detect_trigger_id = false;
};

#endif
