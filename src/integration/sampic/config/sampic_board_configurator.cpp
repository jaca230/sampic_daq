// sampic_board_configurator.cpp
#include "integration/sampic/config/sampic_board_configurator.h"
#include "integration/sampic/config/sampic_chip_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

// ------------------- Ctor -------------------
SampicBoardConfigurator::SampicBoardConfigurator(int boardIdx,
                                                 CrateInfoStruct& info,
                                                 CrateParamStruct& params,
                                                 SampicFrontEndConfig& config)
    : boardIdx_(boardIdx), info_(info), params_(params), config_(config) {}

// ------------------- Apply (board-level) -------------------
void SampicBoardConfigurator::apply() {
    spdlog::debug("Applying FEB {} settings...", boardIdx_);

    setGlobalTrigger();
    setLevel2ExtTrigGate();
    setLevel2Coincidence();

    applyChips();

    spdlog::debug("Finished applying FEB {}.", boardIdx_);
}

// ------------------- Settings -------------------
void SampicBoardConfigurator::setGlobalTrigger() {
    spdlog::trace("FEB {}: SetFrontEndBoardGlobalTriggerOption={}", 
                  boardIdx_, (int)config_.global_trigger_option);

    auto rc = SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(&info_, &params_,
                                                              boardIdx_,
                                                              config_.global_trigger_option);
    check(rc, "SetFrontEndBoardGlobalTriggerOption");
}

void SampicBoardConfigurator::setLevel2ExtTrigGate() {
    spdlog::trace("FEB {}: SetLevel2ExtTrigGate={}", 
                  boardIdx_, (int)config_.level2_ext_trig_gate);

    auto rc = SAMPIC256CH_SetLevel2ExtTrigGate(&info_, &params_,
                                               boardIdx_,
                                               config_.level2_ext_trig_gate);
    check(rc, "SetLevel2ExtTrigGate");
}

void SampicBoardConfigurator::setLevel2Coincidence() {
    spdlog::trace("FEB {}: SetLevel2CoincidenceModeWithExtTrigGate={}", 
                  boardIdx_, config_.level2_coincidence_ext_gate);

    auto rc = SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(&info_, &params_,
                                                                  boardIdx_,
                                                                  config_.level2_coincidence_ext_gate);
    check(rc, "SetLevel2CoincidenceModeWithExtTrigGate");
}

// ------------------- Chips descend -------------------
void SampicBoardConfigurator::applyChips() {
    spdlog::debug("FEB {}: applying {} chips...", boardIdx_, config_.sampics.size());
    for (auto& [chipKey, chipCfg] : config_.sampics) {
        int chipIdx = indexFromKey(chipKey);
        spdlog::debug("  → Apply chip '{}'(index={})", chipKey, chipIdx);
        SampicChipConfigurator chip(boardIdx_, chipIdx, info_, params_, chipCfg);
        chip.apply();
    }
}

// ------------------- Utility -------------------
void SampicBoardConfigurator::check(SAMPIC256CH_ErrCode code, const std::string& what) {
    if (code != SAMPIC256CH_Success) {
        spdlog::error("SAMPIC error (FEB {}) in {} (code={})",
                      boardIdx_, what, static_cast<int>(code));
        throw std::runtime_error("SAMPIC error in " + what +
                                 " (code " + std::to_string(code) + ")");
    }
}

int SampicBoardConfigurator::indexFromKey(const std::string& key) {
    size_t pos = key.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        throw std::runtime_error("Invalid config key: " + key);
    }
    return std::stoi(key.substr(pos + 1));
}
