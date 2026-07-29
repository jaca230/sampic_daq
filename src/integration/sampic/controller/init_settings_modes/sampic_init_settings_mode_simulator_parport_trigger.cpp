#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_simulator_parport_trigger.h"

#include <spdlog/spdlog.h>

int SampicInitSettingsModeSimulatorParportTrigger::initialize()
{
    spdlog::info("Simulator+parport init mode: skipping hardware initialisation");

    info_.NbOfFeBoards = 1;
    params_.CommonParams.NbOfSamplesToRead = MAX_NB_OF_SAMPLES;
    params_.CommonParams.NbOfTriggersPerEvent = 1;

    eventBuffer_ = nullptr;
    mlFrames_ = nullptr;

    return SAMPIC256CH_Success;
}
