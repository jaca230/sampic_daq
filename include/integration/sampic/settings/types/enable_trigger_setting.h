#ifndef SAMPIC_DAQ_ENABLE_TRIGGER_SETTING_H
#define SAMPIC_DAQ_ENABLE_TRIGGER_SETTING_H

struct EnableTriggerSetting {
    bool use_external = false;
    bool open_gate_on_external = false;
    int external_gate = 8;
};

#endif
