#include "integration/sampic/settings/descriptors/builtin_setting_dependencies.h"

namespace {

class FebHardwareSettings {
public:
    static void registerSettings(SampicHardwareRegistry& registry);
};

void FebHardwareSettings::registerSettings(SampicHardwareRegistry& registry) {
    // FEB enable: includes or excludes this front-end board.
    registry.registerSetting<bool>(
        "feb.enabled", HardwareLevel::Feb, "enabled", true, 0, HardwareSettingValidation::any,
        [](const auto&, auto&) { return true; },
        [](const auto&, bool, auto&) {});

    // Global trigger option: selects the FEB-wide trigger routing behavior.
    registry.registerSetting<int>(
        "feb.global_trigger_option", HardwareLevel::Feb, "global_trigger_option", 0,
        10, [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            FebGlobalTrigger_t value{}; SampicVendorCall::check(
                SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption(
                    &c.params, a.feb, &value),
                "GetFrontEndBoardGlobalTriggerOption", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(
                &c.info, &c.params, a.feb, static_cast<FebGlobalTrigger_t>(value)),
                "SetFrontEndBoardGlobalTriggerOption", a);
        });

    // Level-2 trigger logic: configures multiplicity, gate, and channel mask.
    registry.registerSetting<Level2TriggerLogicSetting>(
        "feb.level2_trigger_logic", HardwareLevel::Feb, "level2_trigger_logic", {},
        30, [](const auto& value) {
            HardwareSettingValidation::range(value.sel_input0, 0, 3); HardwareSettingValidation::range(value.sel_input1, 0, 3);
            HardwareSettingValidation::range(value.sel_input2, 0, 3); HardwareSettingValidation::range(value.sel_input3, 0, 3);
            HardwareSettingValidation::range(value.layer1_logic0, 0, 5); HardwareSettingValidation::range(value.layer1_logic1, 0, 5);
            HardwareSettingValidation::range(value.layer1_logic2, 0, 5); HardwareSettingValidation::range(value.layer2_logic0, 0, 5);
            HardwareSettingValidation::range(value.layer2_logic1, 0, 5); HardwareSettingValidation::range(value.layer3_logic, 0, 5);
        },
        [](const auto& a, auto& c) {
            TriggerLogicParamStruct value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel2TriggerLogic(
                &c.params, a.feb, &value), "GetLevel2TriggerLogic", a);
            return Level2TriggerLogicConverter::fromVendor(value);
        },
        [](const auto& a, const auto& value, auto& c) {
            if (!value.apply) return;
            SampicVendorCall::check(SAMPIC256CH_SetLevel2TriggerLogic(
                &c.info, &c.params, a.feb, Level2TriggerLogicConverter::toVendor(value)),
                "SetLevel2TriggerLogic", a);
        });

    // Level-2 external-trigger gate: sets the external gate duration.
    registry.registerSetting<int>(
        "feb.level2_external_trigger_gate", HardwareLevel::Feb,
        "level2_ext_trig_gate", 8, 40, HardwareSettingValidation::byteRange,
        [](const auto& a, auto& c) {
            unsigned char value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel2ExtTrigGate(
                &c.params, a.feb, &value), "GetLevel2ExtTrigGate", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel2ExtTrigGate(
                &c.info, &c.params, a.feb, static_cast<unsigned char>(value)),
                "SetLevel2ExtTrigGate", a);
        });

    // Level-2 coincidence gate: requires coincidence with the external gate.
    registry.registerSetting<bool>(
        "feb.level2_coincidence_external_gate", HardwareLevel::Feb,
        "level2_coincidence_ext_gate", false, 50, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate(
                &c.params, a.feb, &value),
                "GetLevel2CoincidenceModeWithExtTrigGate", a);
            return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(
                &c.info, &c.params, a.feb, value),
                "SetLevel2CoincidenceModeWithExtTrigGate", a);
        });

}

SAMPIC_REGISTER_HARDWARE_SETTINGS(FebHardwareSettings);

}  // namespace
