#include "integration/sampic/config/sampic_channel_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

// ------------------- Ctor -------------------
SampicChannelConfigurator::SampicChannelConfigurator(int boardIdx,
                                                     int chipIdx,
                                                     int channelIdx,
                                                     CrateInfoStruct& info,
                                                     CrateParamStruct& params,
                                                     SampicChannelConfig& config)
    : boardIdx_(boardIdx), chipIdx_(chipIdx), channelIdx_(channelIdx),
      info_(info), params_(params), config_(config) {}

// ------------------- Apply -------------------
void SampicChannelConfigurator::apply() {
    spdlog::debug("Applying channel (FEB={}, chip={}, ch={}) settings...",
                  boardIdx_, chipIdx_, channelIdx_);

    setMode();
    setTriggerMode();
    setThreshold();
    setEdge();
    setSourceForCT();
    setPulseMode();

    spdlog::debug("Finished applying channel (FEB={}, chip={}, ch={}).",
                  boardIdx_, chipIdx_, channelIdx_);
}

// ------------------- Settings -------------------
void SampicChannelConfigurator::setMode() {
    Boolean current{};
    check(SAMPIC256CH_GetChannelMode(&params_,
                                     boardIdx_,
                                     chipIdx_ * 16 + channelIdx_,
                                     &current),
          "GetChannelMode");

    if ((bool)current == config_.enabled) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): Mode already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, (bool)current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing Mode {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, (bool)current, config_.enabled);

    check(SAMPIC256CH_SetChannelMode(&info_, &params_,
                                     boardIdx_,
                                     chipIdx_ * 16 + channelIdx_,
                                     config_.enabled),
          "SetChannelMode");
}

void SampicChannelConfigurator::setTriggerMode() {
    SAMPIC_ChannelTriggerMode_t current{};
    check(SAMPIC256CH_GetSampicChannelTriggerMode(&params_,
                                                  boardIdx_, chipIdx_, channelIdx_,
                                                  &current),
          "GetSampicChannelTriggerMode");

    if (current == config_.trigger_mode) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): TriggerMode already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, (int)current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing TriggerMode {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, (int)current, (int)config_.trigger_mode);

    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_,
                                                  boardIdx_, chipIdx_, channelIdx_,
                                                  config_.trigger_mode),
          "SetSampicChannelTriggerMode");
}

void SampicChannelConfigurator::setThreshold() {
    float current{};
    check(SAMPIC256CH_GetSampicChannelInternalThreshold(&params_,
                                                        boardIdx_, chipIdx_, channelIdx_,
                                                        &current),
          "GetSampicChannelInternalThreshold");

    if (current == config_.internal_threshold) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): InternalThreshold already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing InternalThreshold {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, current, config_.internal_threshold);

    check(SAMPIC256CH_SetSampicChannelInternalThreshold(&info_, &params_,
                                                        boardIdx_, chipIdx_, channelIdx_,
                                                        config_.internal_threshold),
          "SetSampicChannelInternalThreshold");
}

void SampicChannelConfigurator::setEdge() {
    EdgeType_t current{};
    check(SAMPIC256CH_GetChannelSelfTriggerEdge(&params_,
                                                boardIdx_, chipIdx_, channelIdx_,
                                                &current),
          "GetChannelSelfTriggerEdge");

    if (current == config_.trigger_edge) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): TriggerEdge already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, (int)current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing TriggerEdge {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, (int)current, (int)config_.trigger_edge);

    check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_,
                                                 boardIdx_, chipIdx_, channelIdx_,
                                                 config_.trigger_edge),
          "SetChannelSelfTriggerEdge");
}

void SampicChannelConfigurator::setSourceForCT() {
    Boolean current{};
    check(SAMPIC256CH_GetSampicChannelSourceForCT(&params_,
                                                  boardIdx_, chipIdx_, channelIdx_,
                                                  &current),
          "GetSampicChannelSourceForCT");

    if ((bool)current == config_.enable_for_central_trigger) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): SourceForCT already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, (bool)current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing SourceForCT {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, (bool)current,
                  config_.enable_for_central_trigger);

    check(SAMPIC256CH_SetSampicChannelSourceForCT(&info_, &params_,
                                                  boardIdx_, chipIdx_, channelIdx_,
                                                  config_.enable_for_central_trigger),
          "SetSampicChannelSourceForCT");
}

void SampicChannelConfigurator::setPulseMode() {
    Boolean current{};
    check(SAMPIC256CH_GetSampicChannelPulseMode(&params_,
                                                boardIdx_, chipIdx_, channelIdx_,
                                                &current),
          "GetSampicChannelPulseMode");

    if ((bool)current == config_.pulse_mode) {
        spdlog::trace("Channel (FEB={}, chip={}, ch={}): PulseMode already {} -> skip",
                      boardIdx_, chipIdx_, channelIdx_, (bool)current);
        return;
    }

    spdlog::trace("Channel (FEB={}, chip={}, ch={}): Changing PulseMode {} -> {}",
                  boardIdx_, chipIdx_, channelIdx_, (bool)current, config_.pulse_mode);

    check(SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_,
                                                boardIdx_, chipIdx_, channelIdx_,
                                                config_.pulse_mode),
          "SetSampicChannelPulseMode");
}

// ------------------- Utility -------------------
void SampicChannelConfigurator::check(SAMPIC256CH_ErrCode code, const std::string& what) {
    if (code != SAMPIC256CH_Success) {
        spdlog::error("SAMPIC error (FEB={}, chip={}, ch={}) in {} (code={})",
                      boardIdx_, chipIdx_, channelIdx_,
                      what, static_cast<int>(code));
        throw std::runtime_error("SAMPIC error in " + what +
                                 " (code " + std::to_string(code) + ")");
    }
}
