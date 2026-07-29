#ifndef SAMPIC_DAQ_SAMPLING_FREQUENCY_SETTING_H
#define SAMPIC_DAQ_SAMPLING_FREQUENCY_SETTING_H

struct SamplingFrequencySetting {
    int frequency_mhz = 6400;
    bool use_external_clock = false;
};

#endif
