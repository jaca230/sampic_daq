#include "integration/sampic/controller/init_settings_modes/simulator/simulator_mode.h"
#include "core/registry/mode/mode_auto_registration.h"

#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(
    SampicInitSettingsModeRegistry,
    SampicInitSettingsModeSimulator,
    SampicInitSettingsModeSimulatorConfig,
    "simulator",
    "Simulator initialization",
    [](const SampicInitSettingsModeSimulatorConfig& config) {
        if (config.simulated_febs <= 0 || config.simulated_febs > 4) {
            throw std::invalid_argument("simulated_febs must be in [1,4]");
        }
    });

int SampicInitSettingsModeSimulator::initialize()
{
    spdlog::info("Simulator init mode: skipping hardware initialisation");

    // Provide minimal dummy values that downstream code expects.
    info_.NbOfFeBoards = config_.simulated_febs;
    params_.CommonParams.NbOfSamplesToRead = MAX_NB_OF_SAMPLES;
    params_.CommonParams.NbOfTriggersPerEvent = 1;

    eventBuffer_ = nullptr;
    mlFrames_ = nullptr;

    return SAMPIC256CH_Success;
}
