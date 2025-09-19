// sampic_channel_configurator.cpp
#include "integration/sampic/sampic_channel_configurator.h"

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

// ------------------- Apply (channel-level) -------------------
void SampicChannelConfigurator::apply() {
    spdlog::debug("Applying channel (FEB={}, chip={}, ch={}) settings...",
                  boardIdx_, chipIdx_, channelIdx_);

    setMode();
    setTriggerMode();
    setThreshold();
    setEdge();
    setExtThreshMode();
    setSourceForCT();
    setPulseMode();

    spdlog::debug("Finished applying channel (FEB={}, chip={}, ch={}).",
                  boardIdx_, chipIdx_, channelIdx_);
}

// ------------------- Settings -------------------
void SampicChannelConfigurator::setMode() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetChannelMode={}",
                  boardIdx_, chipIdx_, channelIdx_, config_.enabled);

    auto rc = SAMPIC256CH_SetChannelMode(&info_, &params_,
                                         boardIdx_,
                                         (chipIdx_ * 16) + channelIdx_,
                                         config_.enabled);
    check(rc, "SetChannelMode");
}

void SampicChannelConfigurator::setTriggerMode() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetSampicChannelTriggerMode={}",
                  boardIdx_, chipIdx_, channelIdx_, (int)config_.trigger_mode);

    auto rc = SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_,
                                                      boardIdx_, chipIdx_, channelIdx_,
                                                      config_.trigger_mode);
    check(rc, "SetSampicChannelTriggerMode");
}

void SampicChannelConfigurator::setThreshold() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetSampicChannelInternalThreshold={}",
                  boardIdx_, chipIdx_, channelIdx_, config_.internal_threshold);

    auto rc = SAMPIC256CH_SetSampicChannelInternalThreshold(&info_, &params_,
                                                            boardIdx_, chipIdx_, channelIdx_,
                                                            config_.internal_threshold);
    check(rc, "SetSampicChannelInternalThreshold");
}

void SampicChannelConfigurator::setEdge() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetChannelSelfTriggerEdge={}",
                  boardIdx_, chipIdx_, channelIdx_, (int)config_.trigger_edge);

    auto rc = SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_,
                                                     boardIdx_, chipIdx_, channelIdx_,
                                                     config_.trigger_edge);
    check(rc, "SetChannelSelflTriggerEdge");
}

void SampicChannelConfigurator::setExtThreshMode() {
    spdlog::trace("Channel (FEB={}, chip={}): SetSampicExternalThresholdMode={}",
                  boardIdx_, chipIdx_, config_.external_threshold_mode);

    auto rc = SAMPIC256CH_SetSampicExternalThresholdMode(&info_, &params_,
                                                         boardIdx_, chipIdx_,
                                                         config_.external_threshold_mode);
    check(rc, "SetSampicExternalThresholdMode");
}

void SampicChannelConfigurator::setSourceForCT() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetSampicChannelSourceForCT={}",
                  boardIdx_, chipIdx_, channelIdx_, config_.enable_for_central_trigger);

    auto rc = SAMPIC256CH_SetSampicChannelSourceForCT(&info_, &params_,
                                                      boardIdx_, chipIdx_, channelIdx_,
                                                      config_.enable_for_central_trigger);
    check(rc, "SetSampicChannelSourceForCT");
}

void SampicChannelConfigurator::setPulseMode() {
    spdlog::trace("Channel (FEB={}, chip={}, ch={}): SetSampicChannelPulseMode={}",
                  boardIdx_, chipIdx_, channelIdx_, config_.pulse_mode);

    auto rc = SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_,
                                                    boardIdx_, chipIdx_, channelIdx_,
                                                    config_.pulse_mode);
    check(rc, "SetSampicChannelPulseMode");
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
