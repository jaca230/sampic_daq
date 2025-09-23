#ifndef SAMPIC_CRATE_CONFIGURATOR_H
#define SAMPIC_CRATE_CONFIGURATOR_H

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_board_configurator.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

class SampicCrateConfigurator {
public:
    SampicCrateConfigurator(CrateInfoStruct& info,
                            CrateParamStruct& params,
                            SampicSystemSettings& settings);

    void apply();

    // Acquisition
    void setSamplingFrequency();     // SAMPIC256CH_SetSamplingFrequency
    void setFramesPerBlock();        // SAMPIC256CH_SetNbOfFramesPerBlock
    void setTOTMode();               // SAMPIC256CH_SetTOTMeasurementMode
    void setADCBits();               // Set_SystemADCNbOfBits
    void setSmartReadMode();         // SAMPIC256CH_SetSmartReadMode

    // External triggers
    void setExternalTriggerType();   // SAMPIC256CH_SetExternalTriggerType
    void setExternalTriggerLevel();  // SAMPIC256CH_SetExternalTriggerSigLevel
    void setExternalTriggerEdge();   // SAMPIC256CH_SetExternalTriggerEdge
    void setMinTriggersPerEvent();   // SAMPIC256CH_SetMinNbOfTriggersPerEvent
    void setLevel2TriggerBuild();    // SAMPIC256CH_SetLevel2TriggerBuildOption
    void setLevel3TriggerBuild();    // SAMPIC256CH_SetLevel3TriggerLogic

    // Gates
    void setPrimitivesGateLength();          // SAMPIC256CH_SetPrimitivesGateLength
    void setLevel2LatencyGateLength();       // SAMPIC256CH_SetLevel2LatencyGateLength
    void setLevel3ExtTrigGate();             // SAMPIC256CH_SetLevel3ExtTrigGate
    void setLevel3CoincidenceWithExtGate();  // SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate

    // Pulser
    void setPulser();                // Pulser settings

    // Sync + corrections
    void setSyncMode();              // SAMPIC256CH_SetCrateSycnhronisationMode
    void setSyncEdge();              // SAMPIC256CH_SetExternalSyncEdge
    void setSyncLevel();             // SAMPIC256CH_SetExternalSyncSigLevel
    void setCorrectionLevels();      // SAMPIC256CH_SetCrateCorrectionLevels

    // Boards
    void applyBoards();

    //Helpers
    void check(SAMPIC256CH_ErrCode code, const std::string& what);
    int indexFromKey(const std::string& key);

private:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    SampicSystemSettings& settings_;
};

#endif
