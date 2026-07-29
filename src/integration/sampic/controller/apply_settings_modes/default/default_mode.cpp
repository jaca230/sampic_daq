#include "integration/sampic/controller/apply_settings_modes/default/default_mode.h"
#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/calibration/calibration_loader.h"
#include "integration/sampic/settings/sampic_hardware_registry.h"
#include <spdlog/spdlog.h>

SAMPIC_REGISTER_MODE(
    SampicApplySettingsModeRegistry,
    SampicApplySettingsModeDefault,
    SampicApplySettingsModeDefaultConfig,
    "default",
    "Apply all registered hardware settings",
    [](const SampicApplySettingsModeDefaultConfig& config) {
        if (config.reload_calibration &&
            config.calibration_directory.empty()) {
            throw std::invalid_argument(
                "calibration directory is required when reload is enabled");
        }
    });

void SampicApplySettingsModeDefault::apply(const ConfigStore& store) {
    spdlog::info("ApplySettingsModeDefault: Applying full crate configuration...");

    try {
        reloadCalibrationIfNeeded();
        SampicHardwareContext context{info_, params_};
        const int febs = info_.NbOfFeBoards > 0 ? info_.NbOfFeBoards : 4;
        const auto& registry = SampicHardwareRegistry::catalog();
        registry.apply(store, hardware_root_, context, {.febs = febs});
        const auto stats = registry.lastApplyStats();

        spdlog::info(
            "ApplySettingsModeDefault: checked {} addressed settings, changed {}, "
            "skipped {}, elapsed={} ms",
            stats.checked, stats.changed, stats.checked - stats.changed,
            stats.elapsed.count());
    } catch (const std::exception& e) {
        spdlog::error("ApplySettingsModeDefault: Exception during apply: {}", e.what());
        throw;
    }
}

void SampicApplySettingsModeDefault::reloadCalibrationIfNeeded() {
    if (!config_.reload_calibration || config_.calibration_directory.empty()) {
        spdlog::debug("ApplySettingsModeDefault: per-run calibration reload disabled");
        return;
    }

    const auto err = CalibrationLoader::load(
        info_, params_, config_.calibration_directory);
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("ApplySettingsModeDefault: Failed to reload calibration files from '{}'"
                     " (err={})",
                     CalibrationLoader::resolveDirectory(
                         config_.calibration_directory).string(),
                     static_cast<int>(err));
        return;
    }

    spdlog::info("ApplySettingsModeDefault: Calibration values reloaded from '{}'.",
                 CalibrationLoader::resolveDirectory(
                     config_.calibration_directory).string());
}
