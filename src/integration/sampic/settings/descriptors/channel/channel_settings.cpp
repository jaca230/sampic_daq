#include "integration/sampic/settings/descriptors/builtin_setting_dependencies.h"

namespace {

class ChannelHardwareSettings {
public:
    static void registerSettings(SampicHardwareRegistry& registry);
};

void ChannelHardwareSettings::registerSettings(SampicHardwareRegistry& registry) {
    // Channel enable: includes or excludes this channel from acquisition.
    registry.registerSetting<bool>(
        "channel.enabled", HardwareLevel::Channel, "enabled", true, 0, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetChannelMode(
                &c.params, a.feb, a.chip * 16 + a.channel, &value),
                "GetChannelMode", a); return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetChannelMode(
                &c.info, &c.params, a.feb, a.chip * 16 + a.channel, value),
                "SetChannelMode", a);
        });

    // Trigger mode: selects the channel's self-trigger behavior.
    registry.registerSetting<int>(
        "channel.trigger_mode", HardwareLevel::Channel, "trigger_mode", 3, 10,
        [](int value) { HardwareSettingValidation::range(value, 0, 3); },
        [](const auto& a, auto& c) {
            SAMPIC_ChannelTriggerMode_t value{};
            SampicVendorCall::check(SAMPIC256CH_GetSampicChannelTriggerMode(
                &c.params, a.feb, a.chip, a.channel, &value),
                "GetSampicChannelTriggerMode", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicChannelTriggerMode(
                &c.info, &c.params, a.feb, a.chip, a.channel,
                static_cast<SAMPIC_ChannelTriggerMode_t>(value)),
                "SetSampicChannelTriggerMode", a);
        });

    // Internal threshold: sets the channel discriminator threshold voltage.
    registry.registerSetting<double>(
        "channel.internal_threshold", HardwareLevel::Channel,
        "internal_threshold", 0.1, 20, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            float value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicChannelInternalThreshold(
                &c.params, a.feb, a.chip, a.channel, &value),
                "GetSampicChannelInternalThreshold", a); return double(value);
        },
        [](const auto& a, double value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicChannelInternalThreshold(
                &c.info, &c.params, a.feb, a.chip, a.channel,
                static_cast<float>(value)), "SetSampicChannelInternalThreshold", a);
        });

    // Trigger edge: selects rising- or falling-edge self-triggering.
    registry.registerSetting<int>(
        "channel.trigger_edge", HardwareLevel::Channel, "trigger_edge", 0, 30,
        [](int value) { HardwareSettingValidation::range(value, 0, 1); },
        [](const auto& a, auto& c) {
            EdgeType_t value{}; SampicVendorCall::check(SAMPIC256CH_GetChannelSelfTriggerEdge(
                &c.params, a.feb, a.chip, a.channel, &value),
                "GetChannelSelfTriggerEdge", a); return int(value);
        },
        [](const auto& a, int value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetChannelSelflTriggerEdge(
                &c.info, &c.params, a.feb, a.chip, a.channel,
                static_cast<EdgeType_t>(value)), "SetChannelSelfTriggerEdge", a);
        });

    // Central-trigger source: includes this channel in trigger primitives.
    registry.registerSetting<bool>(
        "channel.central_trigger_source", HardwareLevel::Channel,
        "enable_for_central_trigger", true, 40, HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicChannelSourceForCT(
                &c.params, a.feb, a.chip, a.channel, &value),
                "GetSampicChannelSourceForCT", a); return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicChannelSourceForCT(
                &c.info, &c.params, a.feb, a.chip, a.channel, value),
                "SetSampicChannelSourceForCT", a);
        });

    // Pulse mode: enables the channel's internal pulse-generation mode.
    registry.registerSetting<bool>(
        "channel.pulse_mode", HardwareLevel::Channel, "pulse_mode", false, 50,
        HardwareSettingValidation::any,
        [](const auto& a, auto& c) {
            Boolean value{}; SampicVendorCall::check(SAMPIC256CH_GetSampicChannelPulseMode(
                &c.params, a.feb, a.chip, a.channel, &value),
                "GetSampicChannelPulseMode", a); return static_cast<bool>(value);
        },
        [](const auto& a, bool value, auto& c) {
            SampicVendorCall::check(SAMPIC256CH_SetSampicChannelPulseMode(
                &c.info, &c.params, a.feb, a.chip, a.channel, value),
                "SetSampicChannelPulseMode", a);
        });

}

SAMPIC_REGISTER_HARDWARE_SETTINGS(ChannelHardwareSettings);

}  // namespace
