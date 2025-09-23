#ifndef SAMPIC_BOARD_CONFIGURATOR_H
#define SAMPIC_BOARD_CONFIGURATOR_H

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_chip_configurator.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicBoardConfigurator {
public:
    SampicBoardConfigurator(int boardIdx,
                            CrateInfoStruct& info,
                            CrateParamStruct& params,
                            SampicFrontEndConfig& config);

    void apply();

    void setGlobalTrigger();     // SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption
    void setLevel2ExtTrigGate(); // SAMPIC256CH_SetLevel2ExtTrigGate
    void setLevel2Coincidence(); // SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate

    void applyChips();

    //Helpers
    void check(SAMPIC256CH_ErrCode code, const std::string& what);
    int indexFromKey(const std::string& key);

private:
    int boardIdx_;
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicFrontEndConfig& config_;
};

#endif
