#include "integration/sampic/controller/apply_settings_modes/simulator/simulator_mode.h"
#include "core/registry/mode/mode_auto_registration.h"

#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(
    SampicApplySettingsModeRegistry,
    SampicApplySettingsModeSimulator,
    SampicApplySettingsModeSimulatorConfig,
    "simulator",
    "Simulator settings application",
    [](const SampicApplySettingsModeSimulatorConfig&) {});

void SampicApplySettingsModeSimulator::apply(const ConfigStore&)
{
    if (config_.log_application)
        spdlog::info("Simulator apply-settings mode: no hardware configuration performed");
}
