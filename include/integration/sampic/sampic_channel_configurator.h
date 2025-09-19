#ifndef SAMPIC_CHANNEL_CONFIGURATOR_H
#define SAMPIC_CHANNEL_CONFIGURATOR_H

#include "integration/sampic/sampic_config.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicChannelConfigurator {
public:
    SampicChannelConfigurator(int boardIdx,
                              int chipIdx,
                              int channelIdx,
                              CrateInfoStruct& info,
                              CrateParamStruct& params,
                              SampicChannelConfig& config);

    void apply();

    void setMode();           // SAMPIC256CH_SetChannelMode
    void setTriggerMode();    // SAMPIC256CH_SetSampicChannelTriggerMode
    void setThreshold();      // SAMPIC256CH_SetSampicChannelInternalThreshold
    void setEdge();           // SAMPIC256CH_SetChannelSelflTriggerEdge
    void setExtThreshMode();  // SAMPIC256CH_SetSampicExternalThresholdMode
    void setSourceForCT();    // SAMPIC256CH_SetSampicChannelSourceForCT
    void setPulseMode();      // SAMPIC256CH_SetSampicChannelPulseMode

private:
    int boardIdx_;
    int chipIdx_;
    int channelIdx_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicChannelConfig& config_;

    void check(SAMPIC256CH_ErrCode code, const std::string& what);
};

#endif
