#ifndef SAMPIC_DAQ_CORRECTION_LEVELS_SETTING_H
#define SAMPIC_DAQ_CORRECTION_LEVELS_SETTING_H

struct CorrectionLevelsSetting {
    bool adc_linearity = false;
    bool time_inl = false;
    bool residual_pedestal = false;
};

#endif
