#ifndef SAMPIC_DAQ_PULSER_SETTING_H
#define SAMPIC_DAQ_PULSER_SETTING_H

struct PulserSetting {
    bool enabled = false;
    int source = 0;
    bool synchronous = true;
};

#endif
