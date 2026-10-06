#include "integration/sampic/settings/descriptors/builtin_setting_dependencies.h"

namespace {

class ChipHardwareSettings {
public:
    static void registerSettings(SampicHardwareRegistry& registry);
};

void ChipHardwareSettings::registerSettings(SampicHardwareRegistry& registry) {
    // SAMPIC enable: includes or excludes this chip from acquisition.
    registry.registerSetting<bool>(
        "chip.enabled", HardwareLevel::Chip, "enabled", true, 0, HardwareSettingValidation::any,
        [](const auto&, auto&) { return true; },
        [](const auto&, bool, auto&) {});

    // Baseline reference: sets the analog baseline voltage for this chip.
    registry.registerSetting<double>(
        "chip.baseline_reference", HardwareLevel::Chip, "baseline_reference", 0.5,
        10, [](double value) {
            if (value < 0.0 || value > 1.6) throw std::invalid_argument("must be in [0,1.6]");
        },
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetBaselineReference(
                &c.params, a.feb, a.chip, &value), "GetBaselineReference", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetBaselineReference(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetBaselineReference", a);
        });

    // External threshold: sets the shared external discriminator voltage.
    registry.registerSetting<double>(
        "chip.external_threshold", HardwareLevel::Chip, "external_threshold", 0.1,
        20, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicExternalThreshold(
                &c.params, a.feb, a.chip, &value), "GetSampicExternalThreshold", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicExternalThreshold(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicExternalThreshold", a);
        });

    // External-threshold mode: selects external instead of per-channel thresholds.
    registry.registerSetting<bool>(
        "chip.external_threshold_mode", HardwareLevel::Chip,
        "external_threshold_mode", false, 30, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicExternalThresholdMode(
                &c.params, a.feb, a.chip, &value), "GetSampicExternalThresholdMode", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicExternalThresholdMode(
                &c.info, &c.params, a.feb, a.chip, value),
                "SetSampicExternalThresholdMode", a);
        });

    // TOT range: selects the time-over-threshold measurement range.
    registry.registerSetting<int>(
        "chip.tot_range", HardwareLevel::Chip, "tot_range", 2, 40,
        [](int value) { HardwareSettingValidation::range(value, 0, 4); },
        [](const auto& a, auto& c) {
            SAMPIC_TOTRange_t value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicTOTRange(
                &c.params, a.feb, a.chip, &value), "GetSampicTOTRange", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicTOTRange(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SAMPIC_TOTRange_t>(value)), "SetSampicTOTRange", a);
        });

    // TOT filter: configures minimum pulse width and filter capacitor range.
    registry.registerSetting<TotFilterSetting>(
        "chip.tot_filter", HardwareLevel::Chip, "tot_filter", {}, 50,
        [](const auto& value) {
            if (value.minimum_width_ns < 0) throw std::invalid_argument("width must be non-negative");
        },
        [](const auto& a, auto& c) {
            Boolean enabled{}, wide{}; float width{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicTOTFilterParams(
                &c.params, a.feb, a.chip, &enabled, &wide, &width),
                "GetSampicTOTFilterParams", a);
            return TotFilterSetting{static_cast<bool>(enabled),
                                    static_cast<bool>(wide), double(width)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicTOTFilterParams(
                &c.info, &c.params, a.feb, a.chip, value.enabled,
                value.wide_capacitor, static_cast<float>(value.minimum_width_ns)),
                "SetSampicTOTFilterParams", a);
        });

    // Post-trigger: enables and selects the post-trigger sampling delay.
    registry.registerSetting<PostTriggerSetting>(
        "chip.post_trigger", HardwareLevel::Chip, "post_trigger", {}, 60,
        [](const auto& value) {
            if (value.value < 0 || value.value > 7)
                throw std::invalid_argument("value must be in [0,7]");
        },
        [](const auto& a, auto& c) {
            Boolean enabled{}; int value{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicPostTrigParams(
                &c.params, a.feb, a.chip, &enabled, &value),
                "GetSampicPostTrigParams", a);
            return PostTriggerSetting{static_cast<bool>(enabled), value};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicPostTrigParams(
                &c.info, &c.params, a.feb, a.chip, value.enabled, value.value),
                "SetSampicPostTrigParams", a);
        });

    // Central-trigger mode: selects how this chip participates in triggering.
    registry.registerSetting<int>(
        "chip.central_trigger_mode", HardwareLevel::Chip, "central_trigger_mode", 0,
        70, [](int value) { HardwareSettingValidation::range(value, 0, 2); },
        [](const auto& a, auto& c) {
            SampicCentralTriggerMode_t value{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicCentralTriggerMode(
                &c.params, a.feb, a.chip, &value), "GetSampicCentralTriggerMode", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicCentralTriggerMode(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SampicCentralTriggerMode_t>(value)),
                "SetSampicCentralTriggerMode", a);
        });

    // Central-trigger effect: selects the action taken on a central trigger.
    registry.registerSetting<int>(
        "chip.central_trigger_effect", HardwareLevel::Chip,
        "central_trigger_effect", 0, 80,
        [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            SampicCentralTriggerEffect_t value{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicCentralTriggerEffect(
                &c.params, a.feb, a.chip, &value), "GetSampicCentralTriggerEffect", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicCentralTriggerEffect(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SampicCentralTriggerEffect_t>(value)),
                "SetSampicCentralTriggerEffect", a);
        });

    // Central-trigger primitives: configures primitive mode and gate length.
    registry.registerSetting<CentralTriggerPrimitivesSetting>(
        "chip.central_trigger_primitives", HardwareLevel::Chip,
        "central_trigger_primitives", {}, 90,
        [](const auto& value) {
            HardwareSettingValidation::range(value.mode, 0, 1);
            HardwareSettingValidation::range(value.gate_length, 0, 7);
        },
        [](const auto& a, auto& c) {
            SAMPIC_CT_PrimitivesMode_t mode{}; int length{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions(
                &c.params, a.feb, a.chip, &mode, &length),
                "GetSampicCentralTriggerPrimitivesOptions", a);
            return CentralTriggerPrimitivesSetting{int(mode), length};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SAMPIC_CT_PrimitivesMode_t>(value.mode), value.gate_length),
                "SetSampicCentralTriggerPrimitivesOptions", a);
        });

    // Trigger option: selects the chip's trigger source option.
    registry.registerSetting<int>(
        "chip.trigger_option", HardwareLevel::Chip, "trigger_option", 0, 100,
        [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            SampicTriggerOption_t value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicTriggerOption(
                &c.params, a.feb, a.chip, &value), "GetSampicTriggerOption", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicTriggerOption(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SampicTriggerOption_t>(value)),
                "SetSampicTriggerOption", a);
        });

    // Trigger enable: configures external gating and gate-open behavior.
    registry.registerSetting<EnableTriggerSetting>(
        "chip.enable_trigger", HardwareLevel::Chip, "enable_trigger", {}, 110,
        [](const auto& value) { HardwareSettingValidation::byteRange(value.external_gate); },
        [](const auto& a, auto& c) {
            Boolean external{}, open{}; unsigned char gate{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicEnableTriggerMode(
                &c.params, a.feb, a.chip, &external, &open, &gate),
                "GetSampicEnableTriggerMode", a);
            return EnableTriggerSetting{static_cast<bool>(external),
                                        static_cast<bool>(open), int(gate)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicEnableTriggerMode(
                &c.info, &c.params, a.feb, a.chip, value.use_external,
                value.open_gate_on_external,
                static_cast<unsigned char>(value.external_gate)),
                "SetSampicEnableTriggerMode", a);
        });

    // Common dead time: applies one shared dead-time state to all channels.
    registry.registerSetting<bool>(
        "chip.common_dead_time", HardwareLevel::Chip, "common_dead_time", false,
        120, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicCommonDeadTimeMode(
                &c.params, a.feb, a.chip, &value), "GetSampicCommonDeadTimeMode", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicCommonDeadTimeMode(
                &c.info, &c.params, a.feb, a.chip, value),
                "SetSampicCommonDeadTimeMode", a);
        });

    // Pulser width: sets the width of internally generated test pulses.
    registry.registerSetting<int>(
        "chip.pulser_width", HardwareLevel::Chip, "pulser_width", 10, 130,
        HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicPulserWidth(
                &c.params, a.feb, a.chip, &value), "GetSampicPulserWidth", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicPulserWidth(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<unsigned char>(value)), "SetSampicPulserWidth", a);
        });

    // ADC ramp: sets the ADC calibration ramp voltage.
    registry.registerSetting<double>(
        "chip.adc_ramp", HardwareLevel::Chip, "adc_ramp_value", 0.045, 140, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicADCRampValue(
                &c.params, a.feb, a.chip, &value), "GetSampicADCRampValue", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicADCRampValue(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicADCRampValue", a);
        });

    // DLL DAC: sets the delay-locked-loop control voltage.
    registry.registerSetting<double>(
        "chip.vdac_dll", HardwareLevel::Chip, "vdac_dll_value", 1.1, 150, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicVdacDLLValue(
                &c.params, a.feb, a.chip, &value), "GetSampicVdacDLLValue", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicVdacDLLValue(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicVdacDLLValue", a);
        });

    // DLL continuity DAC: sets the continuity correction control voltage.
    registry.registerSetting<double>(
        "chip.vdac_dll_continuity", HardwareLevel::Chip, "vdac_dll_continuity",
        1.1, 160, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicVdacDLLContinuity(
                &c.params, a.feb, a.chip, &value),
                "GetSampicVdacDLLContinuity", a); return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicVdacDLLContinuity(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicVdacDLLContinuity", a);
        });

    // Ring-oscillator DAC: sets the oscillator control voltage.
    registry.registerSetting<double>(
        "chip.vdac_rosc", HardwareLevel::Chip, "vdac_rosc", 1.0, 170, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicVdacRosc(
                &c.params, a.feb, a.chip, &value), "GetSampicVdacRosc", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicVdacRosc(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicVdacRosc", a);
        });

    // DLL speed mode: selects the delay-locked-loop operating speed.
    registry.registerSetting<int>(
        "chip.dll_speed_mode", HardwareLevel::Chip, "dll_speed_mode", 3, 180,
        [](int value) { HardwareSettingValidation::range(value, 0, 3); },
        [](const auto& a, auto& c) {
            SampicDLLModeType_t value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicDLLSpeedMode(
                &c.params, a.feb, a.chip, &value), "GetSampicDLLSpeedMode", a);
            return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicDLLSpeedMode(
                &c.info, &c.params, a.feb, a.chip,
                static_cast<SampicDLLModeType_t>(value)),
                "SetSampicDLLSpeedMode", a);
        });

    // Overflow DAC: sets the waveform overflow protection threshold.
    registry.registerSetting<double>(
        "chip.overflow_dac", HardwareLevel::Chip, "overflow_dac_value", 1.0, 190,
        HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicOverflowDacValue(
                &c.params, a.feb, a.chip, &value), "GetSampicOverflowDacValue", a);
            return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicOverflowDacValue(
                &c.info, &c.params, a.feb, a.chip, static_cast<float>(value)),
                "SetSampicOverflowDacValue", a);
        });

    // LVDS low-current mode: reduces current used by the LVDS outputs.
    registry.registerSetting<bool>(
        "chip.lvds_low_current", HardwareLevel::Chip, "lvds_low_current_mode", true,
        200, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicLvdsLowCurrentMode(
                &c.params, a.feb, a.chip, &value), "GetSampicLvdsLowCurrentMode", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicLvdsLowCurrentMode(
                &c.info, &c.params, a.feb, a.chip, value),
                "SetSampicLvdsLowCurrentMode", a);
        });

}

SAMPIC_REGISTER_HARDWARE_SETTINGS(ChipHardwareSettings);

}  // namespace
