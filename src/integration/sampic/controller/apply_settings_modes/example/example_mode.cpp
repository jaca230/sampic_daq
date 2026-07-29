#include "integration/sampic/controller/apply_settings_modes/example/example_mode.h"
#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/settings/sampic_hardware_registry.h"
#include <spdlog/spdlog.h>
#include <unordered_set>

SAMPIC_REGISTER_MODE(
    SampicApplySettingsModeRegistry,
    SampicApplySettingsModeExample,
    SampicApplySettingsModeExampleConfig,
    "example",
    "Example trigger-only settings application",
    [](const SampicApplySettingsModeExampleConfig&) {});

void SampicApplySettingsModeExample::apply(const ConfigStore& store) {
    spdlog::info("ApplySettingsModeExample: Setting trigger options...");

    try {
        SampicHardwareContext context{info_, params_};
        const int febs = info_.NbOfFeBoards > 0 ? info_.NbOfFeBoards : 4;
        const std::unordered_set<std::string> trigger_settings{
            "crate.external_trigger_type",
            "chip.trigger_option",
            "channel.enabled",
            "channel.trigger_mode",
        };
        SampicHardwareRegistry::catalog().applySelected(
            store, hardware_root_, context, {.febs = febs}, trigger_settings);

        spdlog::info("ApplySettingsModeExample: Trigger options set successfully.");

    } catch (const std::exception& e) {
        spdlog::error("ApplySettingsModeExample: Exception during apply: {}", e.what());
        throw;
    }
}
