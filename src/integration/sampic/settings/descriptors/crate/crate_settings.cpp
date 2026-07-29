#include "integration/sampic/settings/descriptors/builtin_setting_dependencies.h"

namespace {

class CrateHardwareSettings {
public:
    static void registerSettings(SampicHardwareRegistry& registry);
};

void CrateHardwareSettings::registerSettings(
    SampicHardwareRegistry& registry) {

    // Sampling frequency: selects the sampling rate and internal/external clock.
    registry.registerSetting<SamplingFrequencySetting>(
        "crate.sampling_frequency", HardwareLevel::Crate, "sampling_frequency",
        {}, 10, [](const auto& value) { HardwareSettingValidation::positive(value.frequency_mhz); },
        [](const auto& a, auto& c) {
            int frequency{}; Boolean external{};
            SampicVendorCall::check(SAMPIC256CH_GetSamplingFrequency(&c.params, &frequency, &external),
                  "GetSamplingFrequency", a);
            return SamplingFrequencySetting{frequency, static_cast<bool>(external)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSamplingFrequency(
                &c.info, &c.params, value.frequency_mhz, value.use_external_clock),
                "SetSamplingFrequency", a);
        });

    // ADC resolution: selects the number of active ADC conversion bits.
    registry.registerSetting<int>(
        "crate.adc_bits", HardwareLevel::Crate, "adc_bits", 11, 20,
        [](int value) { HardwareSettingValidation::range(value, 8, 11); },
        [](const auto& a, auto& c) {
            int value{}; SampicVendorCall::check(Get_SystemADCNbOfBits(&c.params, &value),
                               "Get_SystemADCNbOfBits", a); return value;
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(Set_SystemADCNbOfBits(&c.info, &c.params, value),
                  "Set_SystemADCNbOfBits", a);
        });

    // Frames per block: controls how many hardware frames form a readout block.
    registry.registerSetting<int>(
        "crate.frames_per_block", HardwareLevel::Crate, "frames_per_block", 1, 30,
        HardwareSettingValidation::positive,
        [](const auto& a, auto& c) {
            int value{}; SampicVendorCall::check(SAMPIC256CH_GetNbOfFramesPerBlock(&c.params, &value),
                               "GetNbOfFramesPerBlock", a); return value;
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetNbOfFramesPerBlock(&c.info, &c.params, value),
                  "SetNbOfFramesPerBlock", a);
        });

    // TOT measurement: enables time-over-threshold acquisition globally.
    registry.registerSetting<bool>(
        "crate.tot_measurement", HardwareLevel::Crate, "enable_tot", false, 40,
        HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetTOTMeasurementMode(&c.params, &value),
                                   "GetTOTMeasurementMode", a); return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetTOTMeasurementMode(&c.info, &c.params, value),
                  "SetTOTMeasurementMode", a);
        });

    // Smart read: limits waveform readout to a configured offset and length.
    registry.registerSetting<SmartReadSetting>(
        "crate.smart_read", HardwareLevel::Crate, "smart_read", {}, 50,
        [](const auto& value) {
            HardwareSettingValidation::nonnegative(value.read_offset); HardwareSettingValidation::positive(value.samples_to_read);
        },
        [](const auto& a, auto& c) {
            Boolean enabled{}; int samples{}; int offset{};
            SampicVendorCall::check(SAMPIC256CH_GetSmartReadMode(&c.params, &enabled, &samples, &offset),
                  "GetSmartReadMode", a);
            return SmartReadSetting{static_cast<bool>(enabled), offset, samples};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSmartReadMode(
                &c.info, &c.params, value.enabled, value.samples_to_read, value.read_offset),
                "SetSmartReadMode", a);
        });

    // Automatic conversion: starts ADC conversion without an explicit command.
    registry.registerSetting<bool>(
        "crate.auto_conversion", HardwareLevel::Crate, "auto_conversion", true, 60,
        HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetAutoConversionMode(&c.params, &value),
                                   "GetAutoConversionMode", a); return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetAutoConversionMode(&c.info, &c.params, value),
                  "SetAutoConversionMode", a);
        });

    // Conversion length: sets the ADC conversion timing length.
    registry.registerSetting<int>(
        "crate.conversion_length", HardwareLevel::Crate, "conversion_length", 250, 70,
        HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetConversionLength(&c.params, &value),
                                         "GetConversionLength", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetConversionLength(
                &c.info, &c.params, static_cast<unsigned char>(value)),
                "SetConversionLength", a);
        });

    // External trigger type: selects software, oscillator, or external signal.
    registry.registerSetting<int>(
        "crate.external_trigger_type", HardwareLevel::Crate, "external_trigger_type", 0,
        100, [](int value) {
            if (value != 0 && value != 2 && value != 4)
                throw std::invalid_argument("must be SOFTWARE(0), INTERNAL_OSC(2), or EXT_SIG(4)");
        },
        [](const auto& a, auto& c) {
            ExternalTriggerType_t value{}; SampicVendorCall::check(SAMPIC256CH_GetExternalTriggerType(
                &c.params, &value), "GetExternalTriggerType", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalTriggerType(
                &c.info, &c.params, static_cast<ExternalTriggerType_t>(value)),
                "SetExternalTriggerType", a);
        });

    // External trigger level: selects the electrical signal-level convention.
    registry.registerSetting<int>(
        "crate.external_trigger_signal_level", HardwareLevel::Crate,
        "signal_level", 0, 110,
        [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            SignalLevel_t value{}; SampicVendorCall::check(SAMPIC256CH_GetExternalTriggerSigLevel(
                &c.params, &value), "GetExternalTriggerSigLevel", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalTriggerSigLevel(
                &c.info, &c.params, static_cast<SignalLevel_t>(value)),
                "SetExternalTriggerSigLevel", a);
        });

    // External trigger edge: selects rising- or falling-edge triggering.
    registry.registerSetting<int>(
        "crate.external_trigger_edge", HardwareLevel::Crate, "trigger_edge", 0,
        120, [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            EdgeType_t value{}; SampicVendorCall::check(SAMPIC256CH_GetExternalTriggerEdge(
                &c.params, &value), "GetExternalTriggerEdge", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalTriggerEdge(
                &c.info, &c.params, static_cast<EdgeType_t>(value)),
                "SetExternalTriggerEdge", a);
        });

    // External trigger counter: enables counting and trigger-ID detection.
    registry.registerSetting<ExternalTriggerCounterSetting>(
        "crate.external_trigger_counter", HardwareLevel::Crate,
        "external_trigger_counter", {}, 130, [](const auto&) {},
        [](const auto& a, auto& c) {
            Boolean enabled{}, detect{};
            SampicVendorCall::check(SAMPIC256CH_GetExternalTriggerCounterMode(
                &c.params, &enabled, &detect), "GetExternalTriggerCounterMode", a);
            return ExternalTriggerCounterSetting{
                static_cast<bool>(enabled), static_cast<bool>(detect)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalTriggerCounterMode(
                &c.info, &c.params, value.enabled, value.detect_trigger_id),
                "SetExternalTriggerCounterMode", a);
        });

    // Triggers per event: sets the minimum triggers required to build an event.
    registry.registerSetting<int>(
        "crate.minimum_triggers_per_event", HardwareLevel::Crate,
        "triggers_per_event", 127, 140,
        [](int value) {
            if (value < 1 || value > 127) throw std::invalid_argument("must be in [1,127]");
        },
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetMinNbOfTriggersPerEvent(
                &c.params, &value), "GetMinNbOfTriggersPerEvent", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(
                &c.info, &c.params, static_cast<unsigned char>(value)),
                "SetMinNbOfTriggersPerEvent", a);
        });

    // Level-3 trigger build: enables crate-level trigger-logic event building.
    registry.registerSetting<bool>(
        "crate.level3_trigger_build", HardwareLevel::Crate, "level3_trigger_build",
        false, 150, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean enabled{}; TriggerLogicParamStruct logic{};
            SampicVendorCall::check(SAMPIC256CH_GetLevel3TriggerLogic(&c.params, &enabled, &logic),
                  "GetLevel3TriggerLogic", a); return static_cast<bool>(enabled);
        },
        [](const auto& a, bool value, auto& c) {
            Boolean ignored{}; TriggerLogicParamStruct logic{};
            SampicVendorCall::check(SAMPIC256CH_GetLevel3TriggerLogic(&c.params, &ignored, &logic),
                  "GetLevel3TriggerLogic", a);
            SampicVendorCall::check(SAMPIC256CH_SetLevel3TriggerLogic(&c.info, &c.params, value, logic),
                  "SetLevel3TriggerLogic", a);
        });

    // Level-2 trigger build: enables FEB-level trigger-based event building.
    registry.registerSetting<bool>(
        "crate.level2_trigger_build", HardwareLevel::Crate, "level2_trigger_build",
        false, 155, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel2TriggerBuildOption(
                &c.params, &value), "GetLevel2TriggerBuildOption", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel2TriggerBuildOption(&c.info, &c.params, value),
                  "SetLevel2TriggerBuildOption", a);
        });

    // Primitives gate length: sets the coincidence window for primitives.
    registry.registerSetting<int>(
        "crate.primitives_gate_length", HardwareLevel::Crate, "primitives_gate_length",
        5, 160, HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetPrimitivesGateLength(
                &c.params, &value), "GetPrimitivesGateLength", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetPrimitivesGateLength(
                &c.info, &c.params, static_cast<unsigned char>(value)),
                "SetPrimitivesGateLength", a);
        });

    // Level-2 latency gate: sets the latency-compensation gate duration.
    registry.registerSetting<int>(
        "crate.level2_latency_gate_length", HardwareLevel::Crate,
        "latency_gate_length", 3, 170, HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel2LatencyGateLength(
                &c.params, &value), "GetLevel2LatencyGateLength", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel2LatencyGateLength(
                &c.info, &c.params, static_cast<unsigned char>(value)),
                "SetLevel2LatencyGateLength", a);
        });

    // Level-3 external-trigger gate: sets the crate coincidence gate duration.
    registry.registerSetting<int>(
        "crate.level3_external_trigger_gate", HardwareLevel::Crate,
        "level3_ext_trig_gate", 8, 180, HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel3ExtTrigGate(
                &c.params, &value), "GetLevel3ExtTrigGate", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel3ExtTrigGate(
                &c.info, &c.params, static_cast<unsigned char>(value)),
                "SetLevel3ExtTrigGate", a);
        });

    // Level-3 coincidence gate: requires coincidence with the external gate.
    registry.registerSetting<bool>(
        "crate.level3_coincidence_external_gate", HardwareLevel::Crate,
        "level3_coincidence_ext_gate", false, 190, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate(
                &c.params, &value), "GetLevel3CoincidenceModeWithExtTrigGate", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate(
                &c.info, &c.params, value), "SetLevel3CoincidenceModeWithExtTrigGate", a);
        });

    // Pulser mode: enables the pulser and selects its source and synchronization.
    registry.registerSetting<PulserSetting>(
        "crate.pulser", HardwareLevel::Crate, "pulser", {}, 200,
        [](const auto& value) { HardwareSettingValidation::range(value.source, 0, 1); },
        [](const auto& a, auto& c) {
            Boolean enabled{}, synchronous{}; PulserSourceType_t source{};
            SampicVendorCall::check(SAMPIC256CH_GetPulserMode(&c.params, &enabled, &source, &synchronous),
                  "GetPulserMode", a);
            return PulserSetting{static_cast<bool>(enabled), int(source),
                                 static_cast<bool>(synchronous)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetPulserMode(
                &c.info, &c.params, value.enabled,
                static_cast<PulserSourceType_t>(value.source), value.synchronous),
                "SetPulserMode", a);
        });

    // Pulser period: sets the automatic test-pulse repetition period.
    registry.registerSetting<int>(
        "crate.pulser_period", HardwareLevel::Crate, "pulser_period", 10, 210,
        HardwareSettingValidation::positive,
        [](const auto& a, auto& c) {
            int value{}; SampicVendorCall::check(SAMPIC256CH_GetAutoPulserPeriod(&c.params, &value),
                               "GetAutoPulserPeriod", a); return value;
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetAutoPulserPeriod(&c.info, &c.params, value),
                  "SetAutoPulserPeriod", a);
        });

    // External synchronization edge: selects the active synchronization edge.
    registry.registerSetting<int>(
        "crate.sync_edge", HardwareLevel::Crate, "sync_edge", 0, 220,
        [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            EdgeType_t value{}; SampicVendorCall::check(SAMPIC256CH_GetExternalSyncEdge(
                &c.params, &value), "GetExternalSyncEdge", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalSyncEdge(
                &c.info, &c.params, static_cast<EdgeType_t>(value)),
                "SetExternalSyncEdge", a);
        });

    // External synchronization level: selects the sync signal-level convention.
    registry.registerSetting<int>(
        "crate.sync_signal_level", HardwareLevel::Crate, "sync_level", 0,
        230, [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            SignalLevel_t value{}; SampicVendorCall::check(SAMPIC256CH_GetExternalSyncSigLevel(
                &c.params, &value), "GetExternalSyncSigLevel", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetExternalSyncSigLevel(
                &c.info, &c.params, static_cast<SignalLevel_t>(value)),
                "SetExternalSyncSigLevel", a);
        });

    // Correction levels: enables ADC, time-INL, and residual-pedestal corrections.
    registry.registerSetting<CorrectionLevelsSetting>(
        "crate.correction_levels", HardwareLevel::Crate, "correction_levels", {},
        240, [](const auto&) {},
        [](const auto& a, auto& c) {
            Boolean adc{}, inl{}, pedestal{};
            SampicVendorCall::check(SAMPIC256CH_GetCrateCorrectionLevels(
                &c.info, &c.params, &adc, &inl, &pedestal),
                "GetCrateCorrectionLevels", a);
            return CorrectionLevelsSetting{
                static_cast<bool>(adc), static_cast<bool>(inl),
                static_cast<bool>(pedestal)};
        },
        [](const auto& a, const auto& value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetCrateCorrectionLevels(
                &c.info, &c.params, value.adc_linearity, value.time_inl,
                value.residual_pedestal), "SetCrateCorrectionLevels", a);
        });

}

SAMPIC_REGISTER_HARDWARE_SETTINGS(CrateHardwareSettings);

}  // namespace
