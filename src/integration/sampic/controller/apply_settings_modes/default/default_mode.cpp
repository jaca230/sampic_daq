#include "integration/sampic/controller/apply_settings_modes/default/default_mode.h"
#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/calibration/calibration_loader.h"
#include "integration/sampic/settings/sampic_hardware_registry.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

SAMPIC_REGISTER_MODE(
    SampicApplySettingsModeRegistry,
    SampicApplySettingsModeDefault,
    SampicApplySettingsModeDefaultConfig,
    "default",
    "Apply all registered hardware settings",
    [](const SampicApplySettingsModeDefaultConfig& config) {
        if ((config.reset_on_relevant_change ||
             config.reload_calibration) &&
            config.calibration_directory.empty()) {
            throw std::invalid_argument(
                "calibration directory is required when reload is enabled");
        }
    });

void SampicApplySettingsModeDefault::apply(const ConfigStore& store) {
    spdlog::info("ApplySettingsModeDefault: Applying full crate configuration...");

    try {
        const bool hardware_was_reset = shouldResetHardware(store);
        if (hardware_was_reset) resetHardware();
        if (shouldReloadCalibration(hardware_was_reset)) reloadCalibration();
        SampicHardwareContext context{info_, params_};
        const int febs = info_.NbOfFeBoards > 0 ? info_.NbOfFeBoards : 4;
        const auto& registry = SampicHardwareRegistry::catalog();
        registry.apply(store, hardware_root_, context, {.febs = febs});
        const auto stats = registry.lastApplyStats();
        settings_applied_since_initialization_ = true;

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

bool SampicApplySettingsModeDefault::shouldResetHardware(
    const ConfigStore& store) const {
    bool should_reset = false;

    if (!settings_applied_since_initialization_) {
        spdlog::debug(
            "ApplySettingsModeDefault: first settings application after "
            "initialization; crate was already reset during initialization");
    } else if (config_.reset_on_relevant_change) {
        const auto frequency_path =
            hardware_root_ + "/sampling_frequency/frequency_mhz";
        const auto external_clock_path =
            hardware_root_ + "/sampling_frequency/use_external_clock";
        const int requested_frequency = store.readValue<int>(frequency_path);
        const bool requested_external_clock =
            store.readValue<bool>(external_clock_path);
        const bool frequency_changed =
            params_.CommonParams.FreqEch != requested_frequency;
        const bool clock_source_changed =
            static_cast<bool>(params_.CommonParams.UseExternalClock) !=
            requested_external_clock;
        if (frequency_changed || clock_source_changed) {
            spdlog::info(
                "ApplySettingsModeDefault: hardware reset required because "
                "sampling setup changed ({} MHz, external_clock={})",
                requested_frequency, requested_external_clock);
            should_reset = true;
        }
    }

    // Add independent reset conditions here as other hardware settings are
    // found to require a reset before they can be applied reliably.
    if (!should_reset) {
        spdlog::debug(
            "ApplySettingsModeDefault: no reset-required setting changed; "
            "preserving crate state");
    }
    return should_reset;
}

void SampicApplySettingsModeDefault::resetHardware() {
    // Reproduce Pascal's systemInit before the registry performs systemSet
    // (crate/readout settings) and systemTrigger (FEB/chip/channel settings).
    // Resetting params with SetDefaultParameters is essential: otherwise the
    // registry could mistake stale software values for applied hardware state
    // and skip required writes after ResetCrate.
    SAMPIC256CH_StopRun(&info_, &params_);
    auto err = SAMPIC256CH_ResetCrate(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        throw std::runtime_error(
            "ResetCrate failed before full settings apply (err=" +
            std::to_string(static_cast<int>(err)) + ")");
    }
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        throw std::runtime_error(
            "SetDefaultParameters failed after crate reset (err=" +
            std::to_string(static_cast<int>(err)) + ")");
    }
    spdlog::info(
        "ApplySettingsModeDefault: crate reset and defaults restored before "
        "ordered settings apply");
}

bool SampicApplySettingsModeDefault::shouldReloadCalibration(
    bool hardware_was_reset) const {
    if (hardware_was_reset) {
        spdlog::info(
            "ApplySettingsModeDefault: calibration reload required after hardware reset");
        return true;
    }

    const auto configured_directory =
        CalibrationLoader::resolveDirectory(config_.calibration_directory);
    const auto loaded_directory = std::filesystem::path(
        info_.CrateCalibInfo.LastCalibDirectory).lexically_normal();
    if (configured_directory != loaded_directory) {
        spdlog::info(
            "ApplySettingsModeDefault: calibration reload required because "
            "directory changed from '{}' to '{}'",
            loaded_directory.string(), configured_directory.string());
        return true;
    }

    if (config_.reload_calibration) {
        spdlog::info(
            "ApplySettingsModeDefault: calibration reload explicitly forced by configuration");
        return true;
    }

    spdlog::debug(
        "ApplySettingsModeDefault: calibration directory unchanged; preserving loaded calibration");
    return false;
}

void SampicApplySettingsModeDefault::reloadCalibration() {
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
