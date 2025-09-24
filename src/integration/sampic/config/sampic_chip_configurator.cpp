// sampic_chip_configurator.cpp
#include "integration/sampic/config/sampic_chip_configurator.h"
#include "integration/sampic/config/sampic_channel_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

// ------------------- Ctor -------------------
SampicChipConfigurator::SampicChipConfigurator(int boardIdx,
                                               int chipIdx,
                                               CrateInfoStruct& info,
                                               CrateParamStruct& params,
                                               SampicChipConfig& config)
    : boardIdx_(boardIdx), chipIdx_(chipIdx),
      info_(info), params_(params), config_(config) {}

// ------------------- Apply (chip-level) -------------------
void SampicChipConfigurator::apply() {
    spdlog::debug("Applying chip (FEB={}, chip={}) settings...", boardIdx_, chipIdx_);

    setBaseline();
    setExtThreshold();
    setTOTRange();
    setPostTrigger();

    setCentralTriggerMode();
    setCentralTriggerEffect();
    setCentralTriggerPrimitives();

    setTriggerOption();
    setTOTFilterParams();

    applyChannels();

    spdlog::debug("Finished applying chip (FEB={}, chip={}).", boardIdx_, chipIdx_);
}

// ------------------- Settings -------------------
void SampicChipConfigurator::setBaseline() {
    spdlog::trace("Chip (FEB={}, chip={}): SetBaselineReference={}",
                  boardIdx_, chipIdx_, config_.baseline_reference);
    auto rc = SAMPIC256CH_SetBaselineReference(&info_, &params_,
                                               boardIdx_, chipIdx_,
                                               config_.baseline_reference);
    check(rc, "SetBaselineReference");
}

void SampicChipConfigurator::setExtThreshold() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicExternalThreshold={}",
                  boardIdx_, chipIdx_, config_.external_threshold);
    auto rc = SAMPIC256CH_SetSampicExternalThreshold(&info_, &params_,
                                                     boardIdx_, chipIdx_,
                                                     config_.external_threshold);
    check(rc, "SetSampicExternalThreshold");
}

void SampicChipConfigurator::setTOTRange() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicTOTRange={}",
                  boardIdx_, chipIdx_, (int)config_.tot_range);
    auto rc = SAMPIC256CH_SetSampicTOTRange(&info_, &params_,
                                            boardIdx_, chipIdx_,
                                            config_.tot_range);
    check(rc, "SetSampicTOTRange");
}

void SampicChipConfigurator::setPostTrigger() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicPostTrigParams enable={}, value={}",
                  boardIdx_, chipIdx_, config_.enable_post_trigger, config_.post_trigger_value);
    auto rc = SAMPIC256CH_SetSampicPostTrigParams(&info_, &params_,
                                                  boardIdx_, chipIdx_,
                                                  config_.enable_post_trigger,
                                                  config_.post_trigger_value);
    check(rc, "SetSampicPostTrigParams");
}

void SampicChipConfigurator::setCentralTriggerMode() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicCentralTriggerMode={}",
                  boardIdx_, chipIdx_, (int)config_.central_trigger_mode);
    auto rc = SAMPIC256CH_SetSampicCentralTriggerMode(&info_, &params_,
                                                      boardIdx_, chipIdx_,
                                                      config_.central_trigger_mode);
    check(rc, "SetSampicCentralTriggerMode");
}

void SampicChipConfigurator::setCentralTriggerEffect() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicCentralTriggerEffect={}",
                  boardIdx_, chipIdx_, (int)config_.central_trigger_effect);
    auto rc = SAMPIC256CH_SetSampicCentralTriggerEffect(&info_, &params_,
                                                        boardIdx_, chipIdx_,
                                                        config_.central_trigger_effect);
    check(rc, "SetSampicCentralTriggerEffect");
}

void SampicChipConfigurator::setCentralTriggerPrimitives() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicCentralTriggerPrimitivesOptions mode={}, gate_len={}",
                  boardIdx_, chipIdx_, (int)config_.primitives_mode, config_.primitives_gate_length);
    auto rc = SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(&info_, &params_,
                                                                   boardIdx_, chipIdx_,
                                                                   config_.primitives_mode,
                                                                   config_.primitives_gate_length);
    check(rc, "SetSampicCentralTriggerPrimitivesOptions");
}

void SampicChipConfigurator::setTriggerOption() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicTriggerOption={}",
                  boardIdx_, chipIdx_, (int)config_.central_trigger_mode);
    auto rc = SAMPIC256CH_SetSampicTriggerOption(&info_, &params_,
                                                 boardIdx_, chipIdx_,
                                                 SAMPIC_TRIGGER_IS_L1); // TODO: map properly if needed
    check(rc, "SetSampicTriggerOption");
}

void SampicChipConfigurator::setTOTFilterParams() {
    spdlog::trace("Chip (FEB={}, chip={}): SetSampicTOTFilterParams en={}, wide={}, minWidthNs={}",
                  boardIdx_, chipIdx_,
                  config_.tot_filter_enable,
                  config_.tot_wide_cap,
                  config_.tot_min_width_ns);
    auto rc = SAMPIC256CH_SetSampicTOTFilterParams(&info_, &params_,
                                                   boardIdx_, chipIdx_,
                                                   config_.tot_filter_enable,
                                                   config_.tot_wide_cap,
                                                   config_.tot_min_width_ns);
    check(rc, "SetSampicTOTFilterParams");
}

// ------------------- Channels descend -------------------
void SampicChipConfigurator::applyChannels() {
    spdlog::debug("Chip (FEB={}, chip={}): applying {} channels...",
                  boardIdx_, chipIdx_, config_.channels.size());
    for (auto& [chKey, chCfg] : config_.channels) {
        int chIdx = indexFromKey(chKey);

        if (!chCfg.enabled) {
            spdlog::info("Skipping channel '{}' (FEB={}, chip={}, idx={}) because it is disabled.",
                         chKey, boardIdx_, chipIdx_, chIdx);
            continue;
        }

        spdlog::debug("  → Apply channel '{}'(index={})", chKey, chIdx);
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
