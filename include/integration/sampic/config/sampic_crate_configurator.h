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
    void setSamplingFrequency();     
    void setFramesPerBlock();        
    void setTOTMode();               
    void setADCBits();               
    void setSmartReadMode();         

    // External triggers
    void setExternalTriggerType();   
    void setExternalTriggerLevel();  
    void setExternalTriggerEdge();   
    void setMinTriggersPerEvent();   
    void setLevel2TriggerBuild();    
    void setLevel3TriggerBuild();    

    // Gates
    void setPrimitivesGateLength();          
    void setLevel2LatencyGateLength();       
    void setLevel3ExtTrigGate();             
    void setLevel3CoincidenceWithExtGate();  

    // Pulser
    void setPulser();                

    // Sync + corrections
    void setSyncEdge();              
    void setSyncLevel();             
    void setCorrectionLevels();      

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
