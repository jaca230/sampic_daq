#include "integration/sampic/controller/sampic_controller.h"

SampicController::SampicController(const SampicControllerConfig& ctrl_cfg,
                                   const SampicCollectorConfig& coll_cfg,
                                   const ConfigStore& store,
                                   std::string init_modes_root,
                                   std::string apply_modes_root,
                                   std::string collector_modes_root,
                                   std::string hardware_root)
    : ctrl_cfg_(ctrl_cfg), coll_cfg_(coll_cfg),
      init_modes_root_(std::move(init_modes_root)),
      apply_modes_root_(std::move(apply_modes_root)),
      collector_modes_root_(std::move(collector_modes_root)),
      hardware_root_(std::move(hardware_root))
{
    buildInitMode(store);
    buildApplyMode(store);

    // Create collector (owns its buffer)
    collector_ = std::make_unique<SampicCollector>(
        coll_cfg_, info_, params_, eventBuffer_, mlFrames_, store, collector_modes_root_);
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
void SampicController::setControllerConfig(const SampicControllerConfig& c,
                                           const ConfigStore& store) {
    if (initialized_ && c.init_mode != ctrl_cfg_.init_mode)
        throw std::logic_error("init_mode cannot change after controller initialization");
    ctrl_cfg_ = c;
    buildApplyMode(store);
}
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

int SampicController::applySettings(const ConfigStore& store) {
    if (!apply_mode_) {
        spdlog::error("Apply mode not configured");
        return -1;
    }
    try {
        // Apply hardware settings
        apply_mode_->apply(store);

        // Rebuild collector with updated config
        stopCollector();
        collector_.reset();
        collector_ = std::make_unique<SampicCollector>(
            coll_cfg_, info_, params_, eventBuffer_, mlFrames_, store, collector_modes_root_);

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
    if (ctrl_cfg_.init_mode == "simulator") {
        run_start_attempted_ = true;
        run_started_ = true;
        return 0;
    }

    // The vendor call performs several hardware writes. A later write may
    // fail after triggers or flow control have already been changed.
    run_start_attempted_ = true;
    auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to start run (err={})", static_cast<int>(err));
        return err;
    }
    run_started_ = true;
    return 0;
}

int SampicController::stopRun() {
    if (!run_started_ && !run_start_attempted_) {
        spdlog::debug("stopRun() called but run was not started — skipping");
        return 0;
    }

    spdlog::info("Stopping SAMPIC run...");
    if (ctrl_cfg_.init_mode == "simulator") {
        run_started_ = false;
        run_start_attempted_ = false;
        return 0;
    }

    auto err = SAMPIC256CH_StopRun(&info_, &params_);
    // Do not retry this automatically after the crate connection is closed.
    // The attempt has nevertheless completed from this process's perspective.
    run_started_ = false;
    run_start_attempted_ = false;
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to stop run (err={})", static_cast<int>(err));
        return err;
    }
    return 0;
}

void SampicController::cleanup() {
    stopCollector();
    stopRun();

    if (!initialized_) {
        spdlog::debug("cleanup() called but controller not initialized — skipping");
        return;
    }

    spdlog::info("Cleaning up SAMPIC resources...");
    if (ctrl_cfg_.init_mode != "simulator") {
        if (eventBuffer_ || mlFrames_) {
            SAMPIC256CH_FreeEventMemory(&eventBuffer_, &mlFrames_);
            eventBuffer_ = nullptr;
            mlFrames_ = nullptr;
        }
        SAMPIC256CH_CloseCrateConnection(&info_);
    }
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

void SampicController::initializeOdb(ConfigStore& store,
                                     const std::string& init_modes_root,
                                     const std::string& apply_modes_root) {
    SampicInitSettingsModeRegistry::catalog().initializeOdb(store, init_modes_root);
    SampicApplySettingsModeRegistry::catalog().initializeOdb(store, apply_modes_root);
}

void SampicController::buildInitMode(const ConfigStore& store) {
    SampicInitSettingsModeContext context{info_, params_, eventBuffer_, mlFrames_};
    init_mode_ = SampicInitSettingsModeRegistry::catalog().create(
        ctrl_cfg_.init_mode, context, store, init_modes_root_);
}

void SampicController::buildApplyMode(const ConfigStore& store) {
    SampicApplySettingsModeContext context{info_, params_, hardware_root_};
    apply_mode_ = SampicApplySettingsModeRegistry::catalog().create(
        ctrl_cfg_.apply_mode, context, store, apply_modes_root_);
}
