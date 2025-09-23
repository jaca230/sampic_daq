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

    void setBaseline();               // SAMPIC256CH_SetBaselineReference
    void setExtThreshold();           // SAMPIC256CH_SetSampicExternalThreshold
    void setTOTRange();               // SAMPIC256CH_SetSampicTOTRange
    void setPostTrigger();            // SAMPIC256CH_SetSampicPostTrigParams

    void setCentralTriggerMode();     // SAMPIC256CH_SetSampicCentralTriggerMode
    void setCentralTriggerEffect();   // SAMPIC256CH_SetSampicCentralTriggerEffect
    void setCentralTriggerPrimitives(); // SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions

    void setTriggerOption();          // SAMPIC256CH_SetSampicTriggerOption
    void setTOTFilterParams();        // SAMPIC256CH_SetSampicTOTFilterParams

    void applyChannels();

private:
    int boardIdx_;
    int chipIdx_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicChipConfig& config_;

    void check(SAMPIC256CH_ErrCode code, const std::string& what);
    int indexFromKey(const std::string& key);
};

#endif
