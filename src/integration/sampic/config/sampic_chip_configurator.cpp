#include "integration/sampic/config/sampic_chip_configurator.h"
#include "integration/sampic/config/sampic_channel_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>

// ------------------- Ctor -------------------
SampicChipConfigurator::SampicChipConfigurator(int boardIdx,
                                               int chipIdx,
                                               CrateInfoStruct& info,
                                               CrateParamStruct& params,
                                               SampicChipConfig& config)
    : boardIdx_(boardIdx), chipIdx_(chipIdx),
      info_(info), params_(params), config_(config) {}

// ------------------- Apply -------------------
void SampicChipConfigurator::apply() {
    spdlog::debug("Applying chip (FEB={}, chip={}) settings...", boardIdx_, chipIdx_);

    setBaseline();
    setExtThreshold();
    setExtThresholdMode();
    setTOTRange();
    setTOTFilterParams();
    setPostTrigger();
    setCentralTriggerMode();
    setCentralTriggerEffect();
    setCentralTriggerPrimitives();
    setTriggerOption();
    setEnableTriggerMode();
    setCommonDeadTime();
    setPulserWidth();
    setAdcRamp();
    setVdacDLL();
    setVdacDLLContinuity();
    setVdacRosc();
    setDllSpeedMode();
    setOverflowDac();
    setLvdsLowCurrent();

    applyChannels();

    spdlog::debug("Finished applying chip (FEB={}, chip={}).", boardIdx_, chipIdx_);
}

// ------------------- Settings -------------------

void SampicChipConfigurator::setBaseline() {
    float current{};
    check(SAMPIC256CH_GetBaselineReference(&params_, boardIdx_, chipIdx_, &current),
          "GetBaselineReference");

    if (std::fabs(current - config_.baseline_reference) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): BaselineReference already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): BaselineReference {} -> {}",
                  boardIdx_, chipIdx_, current, config_.baseline_reference);

    check(SAMPIC256CH_SetBaselineReference(&info_, &params_,
                                           boardIdx_, chipIdx_,
                                           config_.baseline_reference),
          "SetBaselineReference");
}

void SampicChipConfigurator::setExtThreshold() {
    float current{};
    check(SAMPIC256CH_GetSampicExternalThreshold(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicExternalThreshold");

    if (std::fabs(current - config_.external_threshold) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): ExternalThreshold already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): ExternalThreshold {} -> {}",
                  boardIdx_, chipIdx_, current, config_.external_threshold);

    check(SAMPIC256CH_SetSampicExternalThreshold(&info_, &params_,
                                                 boardIdx_, chipIdx_,
                                                 config_.external_threshold),
          "SetSampicExternalThreshold");
}

void SampicChipConfigurator::setExtThresholdMode() {
    Boolean current{};
    check(SAMPIC256CH_GetSampicExternalThresholdMode(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicExternalThresholdMode");

    if ((bool)current == config_.external_threshold_mode) {
        spdlog::trace("Chip (FEB={}, chip={}): ExternalThresholdMode already {} → skip",
                      boardIdx_, chipIdx_, (bool)current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): ExternalThresholdMode {} -> {}",
                  boardIdx_, chipIdx_, (bool)current, config_.external_threshold_mode);

    check(SAMPIC256CH_SetSampicExternalThresholdMode(&info_, &params_,
                                                     boardIdx_, chipIdx_,
                                                     config_.external_threshold_mode),
          "SetSampicExternalThresholdMode");
}

void SampicChipConfigurator::setTOTRange() {
    SAMPIC_TOTRange_t current{};
    check(SAMPIC256CH_GetSampicTOTRange(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicTOTRange");

    if (current == config_.tot_range) {
        spdlog::trace("Chip (FEB={}, chip={}): TOTRange already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): TOTRange {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.tot_range));

    check(SAMPIC256CH_SetSampicTOTRange(&info_, &params_,
                                        boardIdx_, chipIdx_,
                                        config_.tot_range),
          "SetSampicTOTRange");
}

void SampicChipConfigurator::setTOTFilterParams() {
    Boolean en{}, wide{};
    float width{};
    check(SAMPIC256CH_GetSampicTOTFilterParams(&params_, boardIdx_, chipIdx_,
                                               &en, &wide, &width),
          "GetSampicTOTFilterParams");

    if ((bool)en == config_.tot_filter_enable &&
        (bool)wide == config_.tot_wide_cap &&
        std::fabs(width - config_.tot_min_width_ns) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): TOTFilter already en={}, wide={}, width={} → skip",
                      boardIdx_, chipIdx_, (bool)en, (bool)wide, width);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): TOTFilter change to en={}, wide={}, width={}",
                  boardIdx_, chipIdx_,
                  config_.tot_filter_enable,
                  config_.tot_wide_cap,
                  config_.tot_min_width_ns);

    check(SAMPIC256CH_SetSampicTOTFilterParams(&info_, &params_,
                                               boardIdx_, chipIdx_,
                                               config_.tot_filter_enable,
                                               config_.tot_wide_cap,
                                               config_.tot_min_width_ns),
          "SetSampicTOTFilterParams");
}

void SampicChipConfigurator::setPostTrigger() {
    Boolean en{}; int val{};
    check(SAMPIC256CH_GetSampicPostTrigParams(&params_, boardIdx_, chipIdx_, &en, &val),
          "GetSampicPostTrigParams");

    if ((bool)en == config_.enable_post_trigger && val == config_.post_trigger_value) {
        spdlog::trace("Chip (FEB={}, chip={}): PostTrigger already en={}, val={} → skip",
                      boardIdx_, chipIdx_, (bool)en, val);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): PostTrigger change to en={}, val={}",
                  boardIdx_, chipIdx_,
                  config_.enable_post_trigger,
                  config_.post_trigger_value);

    check(SAMPIC256CH_SetSampicPostTrigParams(&info_, &params_,
                                              boardIdx_, chipIdx_,
                                              config_.enable_post_trigger,
                                              config_.post_trigger_value),
          "SetSampicPostTrigParams");
}

void SampicChipConfigurator::setCentralTriggerMode() {
    SampicCentralTriggerMode_t current{};
    check(SAMPIC256CH_GetSampicCentralTriggerMode(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicCentralTriggerMode");

    if (current == config_.central_trigger_mode) {
        spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerMode already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerMode {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.central_trigger_mode));

    check(SAMPIC256CH_SetSampicCentralTriggerMode(&info_, &params_,
                                                  boardIdx_, chipIdx_,
                                                  config_.central_trigger_mode),
          "SetSampicCentralTriggerMode");
}

void SampicChipConfigurator::setCentralTriggerEffect() {
    SampicCentralTriggerEffect_t current{};
    check(SAMPIC256CH_GetSampicCentralTriggerEffect(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicCentralTriggerEffect");

    if (current == config_.central_trigger_effect) {
        spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerEffect already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerEffect {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.central_trigger_effect));

    check(SAMPIC256CH_SetSampicCentralTriggerEffect(&info_, &params_,
                                                    boardIdx_, chipIdx_,
                                                    config_.central_trigger_effect),
          "SetSampicCentralTriggerEffect");
}

void SampicChipConfigurator::setCentralTriggerPrimitives() {
    SAMPIC_CT_PrimitivesMode_t mode{}; int len{};
    check(SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions(&params_, boardIdx_, chipIdx_,
                                                               &mode, &len),
          "GetSampicCentralTriggerPrimitivesOptions");

    if (mode == config_.primitives_mode && len == config_.primitives_gate_length) {
        spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerPrimitives already mode={}, len={} → skip",
                      boardIdx_, chipIdx_, int(mode), len);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): CentralTriggerPrimitives change to mode={}, len={}",
                  boardIdx_, chipIdx_, int(config_.primitives_mode),
                  config_.primitives_gate_length);

    check(SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(&info_, &params_,
                                                               boardIdx_, chipIdx_,
                                                               config_.primitives_mode,
                                                               config_.primitives_gate_length),
          "SetSampicCentralTriggerPrimitivesOptions");
}

void SampicChipConfigurator::setTriggerOption() {
    SampicTriggerOption_t current{};
    check(SAMPIC256CH_GetSampicTriggerOption(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicTriggerOption");

    if (current == config_.trigger_option) {
        spdlog::trace("Chip (FEB={}, chip={}): TriggerOption already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): TriggerOption {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.trigger_option));

    check(SAMPIC256CH_SetSampicTriggerOption(&info_, &params_,
                                             boardIdx_, chipIdx_,
                                             config_.trigger_option),
          "SetSampicTriggerOption");
}

void SampicChipConfigurator::setEnableTriggerMode() {
    Boolean useExt{}, openGate{}; unsigned char extGate{};
    check(SAMPIC256CH_GetSampicEnableTriggerMode(&params_, boardIdx_, chipIdx_,
                                                 &useExt, &openGate, &extGate),
          "GetSampicEnableTriggerMode");

    if ((bool)useExt == config_.enable_trigger_use_external &&
        (bool)openGate == config_.enable_trigger_open_gate_on_ext &&
        extGate == config_.enable_trigger_ext_gate) {
        spdlog::trace("Chip (FEB={}, chip={}): EnableTriggerMode already useExt={}, openGate={}, extGate={} → skip",
                      boardIdx_, chipIdx_, (bool)useExt, (bool)openGate, int(extGate));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): EnableTriggerMode change to useExt={}, openGate={}, extGate={}",
                  boardIdx_, chipIdx_,
                  config_.enable_trigger_use_external,
                  config_.enable_trigger_open_gate_on_ext,
                  int(config_.enable_trigger_ext_gate));

    check(SAMPIC256CH_SetSampicEnableTriggerMode(&info_, &params_,
                                                 boardIdx_, chipIdx_,
                                                 config_.enable_trigger_use_external,
                                                 config_.enable_trigger_open_gate_on_ext,
                                                 config_.enable_trigger_ext_gate),
          "SetSampicEnableTriggerMode");
}

void SampicChipConfigurator::setCommonDeadTime() {
    Boolean current{};
    check(SAMPIC256CH_GetSampicCommonDeadTimeMode(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicCommonDeadTimeMode");

    if ((bool)current == config_.common_dead_time) {
        spdlog::trace("Chip (FEB={}, chip={}): CommonDeadTime already {} → skip",
                      boardIdx_, chipIdx_, (bool)current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): CommonDeadTime {} -> {}",
                  boardIdx_, chipIdx_, (bool)current, config_.common_dead_time);

    check(SAMPIC256CH_SetSampicCommonDeadTimeMode(&info_, &params_,
                                                  boardIdx_, chipIdx_,
                                                  config_.common_dead_time),
          "SetSampicCommonDeadTimeMode");
}

void SampicChipConfigurator::setPulserWidth() {
    unsigned char current{};
    check(SAMPIC256CH_GetSampicPulserWidth(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicPulserWidth");

    if (current == config_.pulser_width) {
        spdlog::trace("Chip (FEB={}, chip={}): PulserWidth already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): PulserWidth {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.pulser_width));

    check(SAMPIC256CH_SetSampicPulserWidth(&info_, &params_,
                                           boardIdx_, chipIdx_,
                                           config_.pulser_width),
          "SetSampicPulserWidth");
}

void SampicChipConfigurator::setAdcRamp() {
    float current{};
    check(SAMPIC256CH_GetSampicADCRampValue(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicADCRampValue");

    if (std::fabs(current - config_.adc_ramp_value) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): ADCRamp already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): ADCRamp {} -> {}",
                  boardIdx_, chipIdx_, current, config_.adc_ramp_value);

    check(SAMPIC256CH_SetSampicADCRampValue(&info_, &params_,
                                            boardIdx_, chipIdx_,
                                            config_.adc_ramp_value),
          "SetSampicADCRampValue");
}

void SampicChipConfigurator::setVdacDLL() {
    float current{};
    check(SAMPIC256CH_GetSampicVdacDLLValue(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicVdacDLLValue");

    if (std::fabs(current - config_.vdac_dll_value) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): VdacDLL already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): VdacDLL {} -> {}",
                  boardIdx_, chipIdx_, current, config_.vdac_dll_value);

    check(SAMPIC256CH_SetSampicVdacDLLValue(&info_, &params_,
                                            boardIdx_, chipIdx_,
                                            config_.vdac_dll_value),
          "SetSampicVdacDLLValue");
}

void SampicChipConfigurator::setVdacDLLContinuity() {
    float current{};
    check(SAMPIC256CH_GetSampicVdacDLLContinuity(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicVdacDLLContinuity");

    if (std::fabs(current - config_.vdac_dll_continuity) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): VdacDLLContinuity already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): VdacDLLContinuity {} -> {}",
                  boardIdx_, chipIdx_, current, config_.vdac_dll_continuity);

    check(SAMPIC256CH_SetSampicVdacDLLContinuity(&info_, &params_,
                                                 boardIdx_, chipIdx_,
                                                 config_.vdac_dll_continuity),
          "SetSampicVdacDLLContinuity");
}

void SampicChipConfigurator::setVdacRosc() {
    float current{};
    check(SAMPIC256CH_GetSampicVdacRosc(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicVdacRosc");

    if (std::fabs(current - config_.vdac_rosc) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): VdacRosc already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): VdacRosc {} -> {}",
                  boardIdx_, chipIdx_, current, config_.vdac_rosc);

    check(SAMPIC256CH_SetSampicVdacRosc(&info_, &params_,
                                        boardIdx_, chipIdx_,
                                        config_.vdac_rosc),
          "SetSampicVdacRosc");
}

void SampicChipConfigurator::setDllSpeedMode() {
    SampicDLLModeType_t current{};
    check(SAMPIC256CH_GetSampicDLLSpeedMode(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicDLLSpeedMode");

    if (current == config_.dll_speed_mode) {
        spdlog::trace("Chip (FEB={}, chip={}): DLLSpeedMode already {} → skip",
                      boardIdx_, chipIdx_, int(current));
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): DLLSpeedMode {} -> {}",
                  boardIdx_, chipIdx_, int(current), int(config_.dll_speed_mode));

    check(SAMPIC256CH_SetSampicDLLSpeedMode(&info_, &params_,
                                            boardIdx_, chipIdx_,
                                            config_.dll_speed_mode),
          "SetSampicDLLSpeedMode");
}

void SampicChipConfigurator::setOverflowDac() {
    float current{};
    check(SAMPIC256CH_GetSampicOverflowDacValue(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicOverflowDacValue");

    if (std::fabs(current - config_.overflow_dac_value) < 1e-6) {
        spdlog::trace("Chip (FEB={}, chip={}): OverflowDac already {} → skip",
                      boardIdx_, chipIdx_, current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): OverflowDac {} -> {}",
                  boardIdx_, chipIdx_, current, config_.overflow_dac_value);

    check(SAMPIC256CH_SetSampicOverflowDacValue(&info_, &params_,
                                                boardIdx_, chipIdx_,
                                                config_.overflow_dac_value),
          "SetSampicOverflowDacValue");
}

void SampicChipConfigurator::setLvdsLowCurrent() {
    Boolean current{};
    check(SAMPIC256CH_GetSampicLvdsLowCurrentMode(&params_, boardIdx_, chipIdx_, &current),
          "GetSampicLvdsLowCurrentMode");

    if ((bool)current == config_.lvds_low_current_mode) {
        spdlog::trace("Chip (FEB={}, chip={}): LvdsLowCurrent already {} → skip",
                      boardIdx_, chipIdx_, (bool)current);
        return;
    }

    spdlog::trace("Chip (FEB={}, chip={}): LvdsLowCurrent {} -> {}",
                  boardIdx_, chipIdx_, (bool)current, config_.lvds_low_current_mode);

    check(SAMPIC256CH_SetSampicLvdsLowCurrentMode(&info_, &params_,
                                                  boardIdx_, chipIdx_,
                                                  config_.lvds_low_current_mode),
          "SetSampicLvdsLowCurrentMode");
}

// ------------------- Channels descend -------------------
void SampicChipConfigurator::applyChannels() {
    spdlog::debug("Chip (FEB={}, chip={}): applying {} channels...",
                  boardIdx_, chipIdx_, config_.channels.size());
    for (auto& [chKey, chCfg] : config_.channels) {
        int chIdx = indexFromKey(chKey);
        spdlog::debug("  → Apply channel '{}' (idx={})", chKey, chIdx);
        SampicChannelConfigurator ch(boardIdx_, chipIdx_, chIdx, info_, params_, chCfg);
        ch.apply();
    }
}

// ------------------- Utility -------------------
void SampicChipConfigurator::check(SAMPIC256CH_ErrCode code, const std::string& what) {
    if (code != SAMPIC256CH_Success) {
        spdlog::error("SAMPIC error (FEB={}, chip={}) in {} (code={})",
                      boardIdx_, chipIdx_, what, static_cast<int>(code));
        throw std::runtime_error("SAMPIC error in " + what +
                                 " (code " + std::to_string(code) + ")");
    }
}

int SampicChipConfigurator::indexFromKey(const std::string& key) {
    size_t pos = key.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        throw std::runtime_error("Invalid config key: " + key);
    }
    return std::stoi(key.substr(pos + 1));
}
