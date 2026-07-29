#ifndef SAMPIC_CONTROLLER_CONFIG_H
#define SAMPIC_CONTROLLER_CONFIG_H

#include <string>

/// Modes for initialization and applying settings
enum class SampicInitSettingsModeType {
    DEFAULT,
    EXAMPLE,
    SIMULATOR,
    SIMULATOR_PP_TRIG
};

enum class SampicApplySettingsModeType {
    DEFAULT,
    EXAMPLE,
    SIMULATOR,
    SIMULATOR_PP_TRIG
};

/// Configuration for the default initialization mode
struct SampicInitSettingsModeDefaultConfig {
    int dummy_param = 0;
};

/// Example initialization mode configuration (placeholder)
struct SampicInitSettingsModeExampleConfig {
    int dummy_param = 0;
};

/// Simulator initialization mode configuration (no-op)
struct SampicInitSettingsModeSimulatorConfig {
    int dummy_param = 0;
};

/// Trigger-driven simulator initialization mode configuration
struct SampicInitSettingsModeSimulatorParportTriggerConfig {
    int dummy_param = 0;
};

/// Configuration for the default apply-settings mode
struct SampicApplySettingsModeDefaultConfig {
    int dummy_param = 0;
};

/// Example apply mode configuration (placeholder)
struct SampicApplySettingsModeExampleConfig {
    int dummy_param = 0;
};

/// Simulator apply-settings mode configuration (no-op)
struct SampicApplySettingsModeSimulatorConfig {
    int dummy_param = 0;
};

/// Trigger-driven simulator apply mode configuration
struct SampicApplySettingsModeSimulatorParportTriggerConfig {
    int dummy_param = 0;
};

/// Top-level controller configuration object
/// Controls which init/apply modes are used and their parameters.
struct SampicControllerConfig {
    // --- Active mode selectors ---
    SampicInitSettingsModeType  init_mode  = SampicInitSettingsModeType::DEFAULT;
    SampicApplySettingsModeType apply_mode = SampicApplySettingsModeType::DEFAULT;

    // --- Per-mode configurations ---
    SampicInitSettingsModeDefaultConfig  init_default_mode;
    SampicInitSettingsModeExampleConfig  init_example_mode;
    SampicInitSettingsModeSimulatorConfig init_simulator_mode;
    SampicInitSettingsModeSimulatorParportTriggerConfig init_simulator_pp_trig_mode;

    SampicApplySettingsModeDefaultConfig apply_default_mode;
    SampicApplySettingsModeExampleConfig apply_example_mode;
    SampicApplySettingsModeSimulatorConfig apply_simulator_mode;
    SampicApplySettingsModeSimulatorParportTriggerConfig apply_simulator_pp_trig_mode;
};

#endif // SAMPIC_CONTROLLER_CONFIG_H
