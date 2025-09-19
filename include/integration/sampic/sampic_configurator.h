#ifndef SAMPIC_CONFIGURATOR_H
#define SAMPIC_CONFIGURATOR_H

#include "integration/sampic/sampic_config.h"
// Sampic
#ifdef __cplusplus
extern "C" {
#endif

#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>

#ifdef __cplusplus
}
#endif
#include <string>
#include <stdexcept>

class SampicConfigurator {
public:
    SampicConfigurator(CrateInfoStruct& info,
                       CrateParamStruct& params,
                       void*& eventBuffer,
                       ML_Frame*& mlFrames);

    void initializeHardware(const SampicSystemSettings& settings);
    void applySettings(const SampicSystemSettings& settings);

private:
    CrateInfoStruct& info_;
    CrateParamStruct& params_;
    void*& eventBuffer_;
    ML_Frame*& mlFrames_;

    void applyAcquisition(const SampicAcquisitionSettings& acq);
    void applyTrigger(const SampicTriggerSettings& trig);
    void applyPulser(const SampicPulserSettings& pulser);
    void applySync(const SampicSyncSettings& sync);
    void applyCalibration(const SampicCalibrationSettings& calib);

    void applyFrontEnds(const SampicFrontEndSettings& febs);
    void applyFrontEnd(int febIdx, const SampicFrontEndConfig& cfg);
    void applyChip(int febIdx, int chipIdx, const SampicChipConfig& cfg);
    void applyChannel(int febIdx, int chipIdx, int chIdx,
                      const SampicChannelConfig& cfg);

    void check(SAMPIC256CH_ErrCode code, const std::string& what);
    int indexFromKey(const std::string& key);
};

#endif // SAMPIC_CONFIGURATOR_H
