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
#include "core/config/config_store.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/collector/sampic_collector.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode.h"

/// High-level orchestrator for SAMPIC system
class SampicController {
public:
    SampicController(const SampicControllerConfig& ctrl_cfg,
                     const SampicCollectorConfig& coll_cfg,
                     const ConfigStore& store,
                     std::string init_modes_root,
                     std::string apply_modes_root,
                     std::string collector_modes_root,
                     std::string hardware_root);

    ~SampicController();

    // ---------------- Config management ----------------
    void setControllerConfig(const SampicControllerConfig& c, const ConfigStore& store);
    SampicControllerConfig& controllerConfig();
    const SampicControllerConfig& controllerConfig() const;

    void setCollectorConfig(const SampicCollectorConfig& c);
    SampicCollectorConfig& collectorConfig();
    const SampicCollectorConfig& collectorConfig() const;

    // ---------------- Lifecycle ----------------
    int initialize();       ///< Initialize hardware (crate connection, params, calib, memory)
    int applySettings(const ConfigStore& store); ///< Apply settings (trigger options etc.)
    int startRun();         ///< Start acquisition
    int stopRun();          ///< Stop acquisition
    void cleanup();         ///< Free resources, close connection

    // ---------------- Collector ----------------
    void startCollector();
    void stopCollector();

    // ---------------- Buffer access ----------------
    SampicEventBuffer& buffer();
    const SampicEventBuffer& buffer() const;
    static void initializeOdb(ConfigStore& store,
                              const std::string& init_modes_root,
                              const std::string& apply_modes_root);

private:
    // Configs
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
    std::string init_modes_root_;
    std::string apply_modes_root_;
    std::string collector_modes_root_;
    std::string hardware_root_;

    void buildInitMode(const ConfigStore& store);
    void buildApplyMode(const ConfigStore& store);

    // State
    bool initialized_{false};
    bool run_started_{false};
    // StartRun() can fail after writing part of the hardware configuration.
    // Keep this separately from run_started_ so cleanup still sends StopRun().
    bool run_start_attempted_{false};
    bool collector_running_{false};
};

#endif // SAMPIC_CONTROLLER_H
