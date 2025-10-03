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

// ------------------- Apply -------------------
void SampicBoardConfigurator::apply() {
    if (!config_.enabled) {
        spdlog::info("FEB {} disabled in config → skipping.", boardIdx_);
        return;
    }

    spdlog::debug("Applying FEB {} settings...", boardIdx_);

    setGlobalTrigger();
    setLevel2TriggerBuild();
    setLevel2ExtTrigGate();
    setLevel2Coincidence();

    applyChips();

    spdlog::debug("Finished applying FEB {}.", boardIdx_);
}

// ------------------- Settings -------------------
void SampicBoardConfigurator::setGlobalTrigger() {
    FebGlobalTrigger_t current{};
    check(SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption(&params_, boardIdx_, &current),
          "GetFrontEndBoardGlobalTriggerOption");

    if (current == config_.global_trigger_option) return;

    check(SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(&info_, &params_,
                                                          boardIdx_,
                                                          config_.global_trigger_option),
          "SetFrontEndBoardGlobalTriggerOption");
}

void SampicBoardConfigurator::setLevel2TriggerBuild() {
    Boolean current{};
    check(SAMPIC256CH_GetLevel2TriggerBuildOption(&params_, &current),
          "GetLevel2TriggerBuildOption");

    Boolean desired = config_.level2_trigger_build ? TRUE : FALSE;
    if (current == desired) return;

    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, desired),
          "SetLevel2TriggerBuildOption");
}

void SampicBoardConfigurator::setLevel2ExtTrigGate() {
    unsigned char current{};
    check(SAMPIC256CH_GetLevel2ExtTrigGate(&params_, boardIdx_, &current),
          "GetLevel2ExtTrigGate");

    if (current == config_.level2_ext_trig_gate) return;

    check(SAMPIC256CH_SetLevel2ExtTrigGate(&info_, &params_,
                                           boardIdx_,
                                           config_.level2_ext_trig_gate),
          "SetLevel2ExtTrigGate");
}

void SampicBoardConfigurator::setLevel2Coincidence() {
    Boolean current{};
    check(SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate(&params_, boardIdx_, &current),
          "GetLevel2CoincidenceModeWithExtTrigGate");

    Boolean desired = config_.level2_coincidence_ext_gate ? TRUE : FALSE;
    if (current == desired) return;

    check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(&info_, &params_,
                                                              boardIdx_,
                                                              desired),
          "SetLevel2CoincidenceModeWithExtTrigGate");
}

// ------------------- Chips descend -------------------
void SampicBoardConfigurator::applyChips() {
    spdlog::debug("FEB {}: applying {} chips...", boardIdx_, config_.sampics.size());
    for (auto& [chipKey, chipCfg] : config_.sampics) {
        int chipIdx = indexFromKey(chipKey);

        if (!chipCfg.enabled) {
            spdlog::info("Skipping chip '{}' (FEB={}, idx={}) — disabled.",
                         chipKey, boardIdx_, chipIdx);
            continue;
        }

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
