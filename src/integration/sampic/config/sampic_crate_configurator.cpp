// sampic_crate_configurator.cpp
#include "integration/sampic/config/sampic_crate_configurator.h"
#include "integration/sampic/config/sampic_board_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

// ------------------- Ctor -------------------
SampicCrateConfigurator::SampicCrateConfigurator(CrateInfoStruct& info,
                                                 CrateParamStruct& params,
                                                 SampicSystemSettings& settings)
    : info_(info), params_(params), settings_(settings) {}

// ------------------- Apply (top-level) -------------------
void SampicCrateConfigurator::apply() {
    spdlog::info("Applying SAMPIC crate settings...");

    // Acquisition
    setSamplingFrequency();
    setADCBits();
    setFramesPerBlock();
    setTOTMode();
    setSmartReadMode();

    // External trigger & build logic
    setExternalTriggerType();
    setExternalTriggerLevel();
    setExternalTriggerEdge();
    setMinTriggersPerEvent();
    setLevel2TriggerBuild();
    setLevel3TriggerBuild();

    // Global gates
    setPrimitivesGateLength();
    setLevel2LatencyGateLength();
    setLevel3ExtTrigGate();
    setLevel3CoincidenceWithExtGate();

    // Pulser
    setPulser();

    // Sync & corrections
    setSyncMode();
    setSyncEdge();
    setSyncLevel();
    setCorrectionLevels();

    // Descend to boards/chips/channels
    applyBoards();

    spdlog::info("SAMPIC crate settings applied.");
}

// ------------------- Acquisition -------------------
void SampicCrateConfigurator::setSamplingFrequency() {
    spdlog::debug("SetSamplingFrequency: {} MS/s (use_external_clock={})",
                  settings_.sampling_frequency_mhz, settings_.use_external_clock);
    auto rc = SAMPIC256CH_SetSamplingFrequency(&info_, &params_,
                                               settings_.sampling_frequency_mhz,
                                               settings_.use_external_clock);
    check(rc, "SetSamplingFrequency");
}

void SampicCrateConfigurator::setFramesPerBlock() {
    spdlog::debug("SetNbOfFramesPerBlock: {}", settings_.frames_per_block);
    auto rc = SAMPIC256CH_SetNbOfFramesPerBlock(&info_, &params_, settings_.frames_per_block);
    check(rc, "SetNbOfFramesPerBlock");
}

void SampicCrateConfigurator::setTOTMode() {
    spdlog::debug("SetTOTMeasurementMode: {}", settings_.enable_tot);
    auto rc = SAMPIC256CH_SetTOTMeasurementMode(&info_, &params_, settings_.enable_tot);
    check(rc, "SetTOTMeasurementMode");
}

void SampicCrateConfigurator::setADCBits() {
    spdlog::debug("Set_SystemADCNbOfBits: {}", settings_.adc_bits);
    auto rc = Set_SystemADCNbOfBits(&info_, &params_, settings_.adc_bits);
    check(rc, "Set_SystemADCNbOfBits");
}

void SampicCrateConfigurator::setSmartReadMode() {
    spdlog::debug("SetSmartReadMode: mode={}, samples={}, offset={}",
                  settings_.smart_read_mode, settings_.samples_to_read, settings_.read_offset);
    auto rc = SAMPIC256CH_SetSmartReadMode(&info_, &params_,
                                           settings_.smart_read_mode,
                                           settings_.samples_to_read,
                                           settings_.read_offset);
    check(rc, "SetSmartReadMode");
}

// ------------------- External trigger + build -------------------
void SampicCrateConfigurator::setExternalTriggerType() {
    spdlog::debug("SetExternalTriggerType: {}", (int)settings_.external_trigger_type);
    auto rc = SAMPIC256CH_SetExternalTriggerType(&info_, &params_, settings_.external_trigger_type);
    check(rc, "SetExternalTriggerType");
}

void SampicCrateConfigurator::setExternalTriggerLevel() {
    spdlog::debug("SetExternalTriggerSigLevel: {}", (int)settings_.signal_level);
    auto rc = SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, settings_.signal_level);
    check(rc, "SetExternalTriggerSigLevel");
}

void SampicCrateConfigurator::setExternalTriggerEdge() {
    spdlog::debug("SetExternalTriggerEdge: {}", (int)settings_.trigger_edge);
    auto rc = SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, settings_.trigger_edge);
    check(rc, "SetExternalTriggerEdge");
}

void SampicCrateConfigurator::setMinTriggersPerEvent() {
    spdlog::debug("SetMinNbOfTriggersPerEvent: {}", (int)settings_.triggers_per_event);
    auto rc = SAMPIC256CH_SetMinNbOfTriggersPerEvent(&info_, &params_,
                                                     (unsigned char)settings_.triggers_per_event);
    check(rc, "SetMinNbOfTriggersPerEvent");
}

void SampicCrateConfigurator::setLevel2TriggerBuild() {
    spdlog::debug("SetLevel2TriggerBuildOption: {}", settings_.level2_trigger_build);
    auto rc = SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, settings_.level2_trigger_build);
    check(rc, "SetLevel2TriggerBuildOption");
}

void SampicCrateConfigurator::setLevel3TriggerBuild() {
    spdlog::debug("SetLevel3TriggerLogic: enable={}", settings_.level3_trigger_build);
    TriggerLogicParamStruct l3params{}; // default params if none provided in settings
    auto rc = SAMPIC256CH_SetLevel3TriggerLogic(&info_, &params_,
                                                settings_.level3_trigger_build, l3params);
    check(rc, "SetLevel3TriggerLogic");
}

// ------------------- Global gates -------------------
void SampicCrateConfigurator::setPrimitivesGateLength() {
    spdlog::debug("SetPrimitivesGateLength: {}", (int)settings_.primitives_gate_length);
    auto rc = SAMPIC256CH_SetPrimitivesGateLength(&info_, &params_,
                                                  (unsigned char)settings_.primitives_gate_length);
    check(rc, "SetPrimitivesGateLength");
}

void SampicCrateConfigurator::setLevel2LatencyGateLength() {
    spdlog::debug("SetLevel2LatencyGateLength: {}", (int)settings_.latency_gate_length);
    auto rc = SAMPIC256CH_SetLevel2LatencyGateLength(&info_, &params_,
                                                     (unsigned char)settings_.latency_gate_length);
    check(rc, "SetLevel2LatencyGateLength");
}

void SampicCrateConfigurator::setLevel3ExtTrigGate() {
    spdlog::debug("SetLevel3ExtTrigGate: {}", (int)settings_.level3_ext_trig_gate);
    auto rc = SAMPIC256CH_SetLevel3ExtTrigGate(&info_, &params_,
                                               (unsigned char)settings_.level3_ext_trig_gate);
    check(rc, "SetLevel3ExtTrigGate");
}

void SampicCrateConfigurator::setLevel3CoincidenceWithExtGate() {
    spdlog::debug("SetLevel3CoincidenceModeWithExtTrigGate: {}", settings_.level3_coincidence_ext_gate);
    auto rc = SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate(&info_, &params_,
                                                                  settings_.level3_coincidence_ext_gate);
    check(rc, "SetLevel3CoincidenceModeWithExtTrigGate");
}

// ------------------- Pulser -------------------
void SampicCrateConfigurator::setPulser() {
    spdlog::debug("SetPulserMode: enable={}, src={}, sync={}",
                  settings_.pulser_enable, (int)settings_.pulser_source, settings_.pulser_synchronous);
    auto rc = SAMPIC256CH_SetPulserMode(&info_, &params_,
                                        settings_.pulser_enable,
                                        settings_.pulser_source,
                                        settings_.pulser_synchronous);
    check(rc, "SetPulserMode");

    spdlog::debug("SetAutoPulserPeriod: {}", settings_.pulser_period);
    rc = SAMPIC256CH_SetAutoPulserPeriod(&info_, &params_, settings_.pulser_period);
    check(rc, "SetAutoPulserPeriod");

    // NOTE: pulser width is a per-chip setting in the C API; apply across all (board, chip).
    for (const auto& [febKey, febCfg] : settings_.front_end_boards) {
        int febIdx = indexFromKey(febKey);
        // We’ll apply the *crate-level* width to every chip unless a chip-level configurator later overrides it.
        for (const auto& [chipKey, chipCfg] : febCfg.sampics) {
            int chipIdx = indexFromKey(chipKey);
            spdlog::trace("SetSampicPulserWidth: feb={}, chip={}, width={}",
                          febIdx, chipIdx, (int)settings_.pulser_width);
            auto rc2 = SAMPIC256CH_SetSampicPulserWidth(&info_, &params_, febIdx, chipIdx,
                                                        (unsigned char)settings_.pulser_width);
            check(rc2, "SetSampicPulserWidth");
        }
    }
}

// ------------------- Sync + corrections -------------------
void SampicCrateConfigurator::setSyncMode() {
    spdlog::debug("SetCrateSycnhronisationMode: sync={}, master={}, coinc={}",
                  settings_.sync_mode, settings_.master_mode, settings_.coincidence_mode);
    auto rc = SAMPIC256CH_SetCrateSycnhronisationMode(&info_, &params_,
                                                      settings_.sync_mode,
                                                      settings_.master_mode,
                                                      settings_.coincidence_mode);
    check(rc, "SetCrateSycnhronisationMode");
}

void SampicCrateConfigurator::setSyncEdge() {
    spdlog::debug("SetExternalSyncEdge: {}", (int)settings_.sync_edge);
    auto rc = SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, settings_.sync_edge);
    check(rc, "SetExternalSyncEdge");
}

void SampicCrateConfigurator::setSyncLevel() {
    spdlog::debug("SetExternalSyncSigLevel: {}", (int)settings_.sync_level);
    auto rc = SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, settings_.sync_level);
    check(rc, "SetExternalSyncSigLevel");
}

void SampicCrateConfigurator::setCorrectionLevels() {
    spdlog::debug("SetCrateCorrectionLevels: adc={}, inl={}, ped={}",
                  settings_.adc_linearity_correction,
                  settings_.time_inl_correction,
                  settings_.residual_pedestal_correction);
    auto rc = SAMPIC256CH_SetCrateCorrectionLevels(&info_, &params_,
                                                   settings_.adc_linearity_correction,
                                                   settings_.time_inl_correction,
                                                   settings_.residual_pedestal_correction);
    check(rc, "SetCrateCorrectionLevels");

    // Optional: auto-load all calibration values if directory provided (non-empty)
    if (!settings_.calibration_directory.empty()) {
        spdlog::debug("LoadAllCalibValuesFromFiles: dir='{}'", settings_.calibration_directory);
        char dirbuf[MAX_PATHNAME_LENGTH] = {0};
        std::snprintf(dirbuf, sizeof(dirbuf), "%s", settings_.calibration_directory.c_str());
        auto rc2 = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dirbuf);
        if (rc2 != SAMPIC256CH_Success) {
            // Not fatal for general operation; log and continue (some users don't ship full calib sets).
            spdlog::warn("LoadAllCalibValuesFromFiles failed (code={}), continuing without file-based calib.",
                         (int)rc2);
        }
    }
}

// ------------------- Boards descend -------------------
void SampicCrateConfigurator::applyBoards() {
    spdlog::debug("Applying front-end boards settings...");
    for (auto& [key, febCfg] : settings_.front_end_boards) {
        int febIdx = indexFromKey(key);
        spdlog::debug("Apply FEB '{}'(index={})", key, febIdx);
        SampicBoardConfigurator feb(febIdx, info_, params_, febCfg);
        feb.apply();
    }
    spdlog::debug("Front-end boards applied.");
}

// ------------------- Utility -------------------
void SampicCrateConfigurator::check(SAMPIC256CH_ErrCode code, const std::string& what) {
    if (code != SAMPIC256CH_Success) {
        spdlog::error("SAMPIC error in {} (code={})", what, static_cast<int>(code));
        throw std::runtime_error("SAMPIC error in " + what +
                                 " (code " + std::to_string(code) + ")");
    }
}

int SampicCrateConfigurator::indexFromKey(const std::string& key) {
    size_t pos = key.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        throw std::runtime_error("Invalid config key: " + key);
    }
    return std::stoi(key.substr(pos + 1));
}
