#include "integration/sampic/controller/sampic_controller.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_default.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_example.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_default.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_example.h"

SampicController::SampicController(const SampicSystemSettings& sys_cfg,
                                   const SampicControllerConfig& ctrl_cfg,
                                   const SampicCollectorConfig& coll_cfg)
    : settings_(sys_cfg),
      ctrl_cfg_(ctrl_cfg),
      coll_cfg_(coll_cfg)
{
    // Select init mode
    switch (ctrl_cfg_.init_mode) {
        case SampicInitSettingsModeType::DEFAULT:
            init_mode_ = std::make_unique<SampicInitSettingsModeDefault>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
        case SampicInitSettingsModeType::EXAMPLE:
            init_mode_ = std::make_unique<SampicInitSettingsModeExample>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
    }

    // Select apply mode
    switch (ctrl_cfg_.apply_mode) {
        case SampicApplySettingsModeType::DEFAULT:
            apply_mode_ = std::make_unique<SampicApplySettingsModeDefault>(
                info_, params_, settings_, ctrl_cfg_);
            break;
        case SampicApplySettingsModeType::EXAMPLE:
            apply_mode_ = std::make_unique<SampicApplySettingsModeExample>(
                info_, params_, settings_, ctrl_cfg_);
            break;
    }

    // Create collector (owns its buffer)
    collector_ = std::make_unique<SampicCollector>(
        coll_cfg_, info_, params_, eventBuffer_, mlFrames_);
}

SampicController::~SampicController() {
    try {
        stopCollector();
        stopRun();
        cleanup();
    } catch (...) {
        // swallow errors in destructor
    }
}

// ---------------- Config management ----------------
void SampicController::setSystemSettings(const SampicSystemSettings& s) { settings_ = s; }
SampicSystemSettings& SampicController::systemSettings() { return settings_; }
const SampicSystemSettings& SampicController::systemSettings() const { return settings_; }

void SampicController::setControllerConfig(const SampicControllerConfig& c) { ctrl_cfg_ = c; }
SampicControllerConfig& SampicController::controllerConfig() { return ctrl_cfg_; }
const SampicControllerConfig& SampicController::controllerConfig() const { return ctrl_cfg_; }

void SampicController::setCollectorConfig(const SampicCollectorConfig& c) { coll_cfg_ = c; }
SampicCollectorConfig& SampicController::collectorConfig() { return coll_cfg_; }
const SampicCollectorConfig& SampicController::collectorConfig() const { return coll_cfg_; }

// ---------------- Lifecycle ----------------
int SampicController::initialize() {
    if (!init_mode_) {
        spdlog::error("Init mode not configured");
        return -1;
    }
    int rc = init_mode_->initialize();
    initialized_ = (rc == SAMPIC256CH_Success);
    return rc;
}

int SampicController::applySettings() {
    if (!apply_mode_) {
        spdlog::error("Apply mode not configured");
        return -1;
    }
    try {
        // Apply hardware settings
        apply_mode_->apply();

        // Rebuild collector with updated config
        stopCollector();
        collector_.reset();
        collector_ = std::make_unique<SampicCollector>(
            coll_cfg_, info_, params_, eventBuffer_, mlFrames_);

        spdlog::info("Collector rebuilt with new configuration");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Apply settings failed: {}", e.what());
        return -1;
    }
}

int SampicController::startRun() {
    if (run_started_) {
        spdlog::warn("startRun() called but run already started");
        return 0;
    }

    spdlog::info("Starting SAMPIC run...");
    auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to start run (err={})", static_cast<int>(err));
        return err;
    }
    run_started_ = true;
    return 0;
}

int SampicController::stopRun() {
    if (!run_started_) {
        spdlog::debug("stopRun() called but run was not started — skipping");
        return 0;
    }

    spdlog::info("Stopping SAMPIC run...");
    auto err = SAMPIC256CH_StopRun(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to stop run (err={})", static_cast<int>(err));
        return err;
    }
    run_started_ = false;
    return 0;
}

void SampicController::cleanup() {
    if (!initialized_) {
        spdlog::debug("cleanup() called but controller not initialized — skipping");
        return;
    }

    spdlog::info("Cleaning up SAMPIC resources...");
    if (eventBuffer_ || mlFrames_) {
        SAMPIC256CH_FreeEventMemory(&eventBuffer_, &mlFrames_);
        eventBuffer_ = nullptr;
        mlFrames_ = nullptr;
    }
    SAMPIC256CH_CloseCrateConnection(&info_);
    initialized_ = false;
}

// ---------------- Collector ----------------
void SampicController::startCollector() {
    if (collector_ && !collector_running_) {
        collector_->start();
        collector_running_ = true;
    }
}
void SampicController::stopCollector() {
    if (collector_ && collector_running_) {
        collector_->stop();
        collector_running_ = false;
    }
}

// ---------------- Buffer access ----------------
SampicEventBuffer& SampicController::buffer() {
    return collector_->buffer();
}
const SampicEventBuffer& SampicController::buffer() const {
    return collector_->buffer();
}
