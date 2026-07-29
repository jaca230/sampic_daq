#ifndef SAMPIC_DAQ_TOT_FILTER_SETTING_H
#define SAMPIC_DAQ_TOT_FILTER_SETTING_H

struct TotFilterSetting {
    bool enabled = false;
    bool wide_capacitor = false;
    double minimum_width_ns = 10.0;
};

#endif
