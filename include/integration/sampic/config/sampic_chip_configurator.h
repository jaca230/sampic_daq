#ifndef SAMPIC_CHIP_CONFIGURATOR_H
#define SAMPIC_CHIP_CONFIGURATOR_H

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_channel_configurator.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicChipConfigurator {
public:
    SampicChipConfigurator(int boardIdx,
                           int chipIdx,
                           CrateInfoStruct& info,
                           CrateParamStruct& params,
                           SampicChipConfig& config);

    void apply();

    // Individual settings
    void setBaseline();
    void setExtThreshold();
    void setExtThresholdMode();
    void setTOTRange();
    void setTOTFilterParams();
    void setPostTrigger();
    void setCentralTriggerMode();
    void setCentralTriggerEffect();
    void setCentralTriggerPrimitives();
    void setTriggerOption();
    void setEnableTriggerMode();
    void setCommonDeadTime();
    void setPulserWidth();
    void setAdcRamp();
    void setVdacDLL();
    void setVdacDLLContinuity();
    void setVdacRosc();
    void setDllSpeedMode();
    void setOverflowDac();
    void setLvdsLowCurrent();

    void applyChannels();

    // Helpers
    void check(SAMPIC256CH_ErrCode code, const std::string& what);
    int indexFromKey(const std::string& key);

private:
    int boardIdx_;
    int chipIdx_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicChipConfig& config_;
};

#endif
