#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_default.h"
#include "integration/sampic/config/sampic_crate_configurator.h"
#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>

void SampicApplySettingsModeDefault::apply() {
    spdlog::info("ApplySettingsModeDefault: Applying full crate configuration...");

    try {
        // Use the configurator to apply *everything* from settings_
        SampicCrateConfigurator crateCfg(info_, params_, settings_);
        crateCfg.apply();
        reloadCalibrationIfNeeded();

        spdlog::info("ApplySettingsModeDefault: All settings applied successfully.");
    } catch (const std::exception& e) {
        spdlog::error("ApplySettingsModeDefault: Exception during apply: {}", e.what());
        throw;
    }
}

void SampicApplySettingsModeDefault::reloadCalibrationIfNeeded() {
    if (settings_.calibration_directory.empty()) {
        spdlog::info("ApplySettingsModeDefault: Calibration reload requested but "
                     "no calibration directory is configured.");
        return;
    }

    std::array<char, MAX_PATHNAME_LENGTH> dir{};
    std::snprintf(dir.data(), dir.size(), "%s", settings_.calibration_directory.c_str());

    const auto err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dir.data());
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("ApplySettingsModeDefault: Failed to reload calibration files from '{}'"
                     " (err={})",
                     settings_.calibration_directory,
                     static_cast<int>(err));
        return;
    }

    spdlog::info("ApplySettingsModeDefault: Calibration values reloaded from '{}'.",
                 settings_.calibration_directory);

    const auto corrErr = SAMPIC256CH_SetCrateCorrectionLevels(
        &info_, &params_,
        settings_.adc_linearity_correction,
        settings_.time_inl_correction,
        settings_.residual_pedestal_correction);

    if (corrErr != SAMPIC256CH_Success) {
        spdlog::warn("ApplySettingsModeDefault: Failed to reapply correction flags after "
                     "calibration reload (err={})",
                     static_cast<int>(corrErr));
    }
}
