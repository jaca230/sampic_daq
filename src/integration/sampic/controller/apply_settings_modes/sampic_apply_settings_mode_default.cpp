#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_default.h"
#include "integration/sampic/config/sampic_crate_configurator.h"
#include <spdlog/spdlog.h>

void SampicApplySettingsModeDefault::apply() {
    spdlog::info("ApplySettingsModeDefault: Applying full crate configuration...");

    try {
        // Use the configurator to apply *everything* from settings_
        SampicCrateConfigurator crateCfg(info_, params_, settings_);
        crateCfg.apply();

        spdlog::info("ApplySettingsModeDefault: All settings applied successfully.");
    } catch (const std::exception& e) {
        spdlog::error("ApplySettingsModeDefault: Exception during apply: {}", e.what());
        throw;
    }
}
