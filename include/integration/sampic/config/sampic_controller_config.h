#ifndef SAMPIC_CONTROLLER_CONFIG_H
#define SAMPIC_CONTROLLER_CONFIG_H

#include <map>
#include <string>
#include <cstddef>

/// Modes for initialization and applying settings
enum class SampicInitSettingsModeType {
    DEFAULT,
    EXAMPLE
};

enum class SampicApplySettingsModeType {
    DEFAULT,
    EXAMPLE
};

/// Configuration for the default initialization mode
struct SampicInitSettingsModeDefaultConfig {
    int dummy_param = 0;
};

/// Example init mode configuration (placeholder)
struct SampicInitSettingsModeExampleConfig {
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

/// Top-level controller configuration object
/// Controls which init/apply modes are used and their parameters.
struct SampicControllerConfig {
    // --- Active mode selectors ---
    SampicInitSettingsModeType  init_mode  = SampicInitSettingsModeType::DEFAULT;
    SampicApplySettingsModeType apply_mode = SampicApplySettingsModeType::DEFAULT;

    // --- Mode-specific configuration maps ---
    std::map<std::string, SampicInitSettingsModeDefaultConfig> init_default_mode {
        {"default_init_mode", {}}
    };

    std::map<std::string, SampicInitSettingsModeExampleConfig> init_example_mode {
        {"example_init_mode", {}}
    };

    std::map<std::string, SampicApplySettingsModeDefaultConfig> apply_default_mode {
        {"default_apply_mode", {}}
    };

    std::map<std::string, SampicApplySettingsModeExampleConfig> apply_example_mode {
        {"example_apply_mode", {}}
    };
};

#endif // SAMPIC_CONTROLLER_CONFIG_H
