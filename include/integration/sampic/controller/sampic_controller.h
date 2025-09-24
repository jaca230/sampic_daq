#ifndef SAMPIC_CONTROLLER_H
#define SAMPIC_CONTROLLER_H

#include <memory>
#include <spdlog/spdlog.h>

// External SAMPIC lib
extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

// Project configs + components
#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/collector/sampic_collector.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"

/// High-level orchestrator for SAMPIC system
class SampicController {
public:
    SampicController(const SampicSystemSettings& sys_cfg,
                     const SampicControllerConfig& ctrl_cfg,
                     const SampicCollectorConfig& coll_cfg);

    ~SampicController();

    // ---------------- Config management ----------------
    void setSystemSettings(const SampicSystemSettings& s);
    SampicSystemSettings& systemSettings();
    const SampicSystemSettings& systemSettings() const;

    void setControllerConfig(const SampicControllerConfig& c);
    SampicControllerConfig& controllerConfig();
    const SampicControllerConfig& controllerConfig() const;

    void setCollectorConfig(const SampicCollectorConfig& c);
    SampicCollectorConfig& collectorConfig();
    const SampicCollectorConfig& collectorConfig() const;

    // ---------------- Lifecycle ----------------
    int initialize();       ///< Initialize hardware (crate connection, params, calib, memory)
    int applySettings();    ///< Apply settings (trigger options etc.)
    int startRun();         ///< Start acquisition
    int stopRun();          ///< Stop acquisition
    void cleanup();         ///< Free resources, close connection

    // ---------------- Collector ----------------
    void startCollector();
    void stopCollector();

    // ---------------- Buffer access ----------------
    SampicEventBuffer& buffer();
    const SampicEventBuffer& buffer() const;

private:
    // Configs
    SampicSystemSettings   settings_;
    SampicControllerConfig ctrl_cfg_;
    SampicCollectorConfig  coll_cfg_;

    // Hardware handles
    CrateInfoStruct info_{};
    CrateParamStruct params_{};
    void* eventBuffer_{nullptr};
    ML_Frame* mlFrames_{nullptr};

    // Collector (owns its buffer)
    std::unique_ptr<SampicCollector> collector_;

    // Init/apply strategies
    std::unique_ptr<SampicInitSettingsMode> init_mode_;
    std::unique_ptr<SampicApplySettingsMode> apply_mode_;

    // State
    bool initialized_{false};
    bool run_started_{false};
    bool collector_running_{false};
};

#endif // SAMPIC_CONTROLLER_H
