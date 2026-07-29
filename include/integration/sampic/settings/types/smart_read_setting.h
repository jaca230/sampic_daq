#ifndef SAMPIC_DAQ_SMART_READ_SETTING_H
#define SAMPIC_DAQ_SMART_READ_SETTING_H

struct SmartReadSetting {
    bool enabled = false;
    int read_offset = 0;
    int samples_to_read = 64;
};

#endif
