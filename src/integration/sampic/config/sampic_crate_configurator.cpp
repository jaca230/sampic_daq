#include "integration/sampic/config/sampic_crate_configurator.h"
#include "integration/sampic/config/sampic_board_configurator.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>
#include <string>

// ------------------- Ctor -------------------
SampicCrateConfigurator::SampicCrateConfigurator(CrateInfoStruct& info,
                                                 CrateParamStruct& params,
                                                 SampicSystemSettings& settings)
    : info_(info), params_(params), settings_(settings) {}

// ------------------- Apply -------------------
void SampicCrateConfigurator::apply() {
    spdlog::info("Applying SAMPIC crate settings...");

    setSamplingFrequency();
    setADCBits();
    setFramesPerBlock();
    setTOTMode();
    setSmartReadMode();

    setExternalTriggerType();
    setExternalTriggerLevel();
    setExternalTriggerEdge();
    setMinTriggersPerEvent();
    setLevel3TriggerBuild();

    setPrimitivesGateLength();
    setLevel2LatencyGateLength();
    setLevel3ExtTrigGate();
    setLevel3CoincidenceWithExtGate();

    setPulser();

    setSyncEdge();
    setSyncLevel();
    setCorrectionLevels();

    applyBoards();

    spdlog::info("SAMPIC crate settings applied.");
}

// ------------------- Acquisition -------------------
void SampicCrateConfigurator::setSamplingFrequency() {
    int current{}; Boolean useExt{};
    check(SAMPIC256CH_GetSamplingFrequency(&params_, &current, &useExt),
          "GetSamplingFrequency");

    if (current == settings_.sampling_frequency_mhz &&
        (bool)useExt == settings_.use_external_clock) {
        spdlog::trace("SamplingFrequency already {} MHz, useExt={} → skip",
                      current, (bool)useExt);
        return;
    }

    spdlog::trace("SamplingFrequency change to {} MHz, useExt={}",
                  settings_.sampling_frequency_mhz, settings_.use_external_clock);

    check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_,
                                           settings_.sampling_frequency_mhz,
                                           settings_.use_external_clock),
          "SetSamplingFrequency");
}

void SampicCrateConfigurator::setFramesPerBlock() {
    int current{};
    check(SAMPIC256CH_GetNbOfFramesPerBlock(&params_, &current),
          "GetNbOfFramesPerBlock");

    if (current == settings_.frames_per_block) {
        spdlog::trace("FramesPerBlock already {} → skip", current);
        return;
    }

    spdlog::trace("FramesPerBlock {} -> {}", current, settings_.frames_per_block);

    check(SAMPIC256CH_SetNbOfFramesPerBlock(&info_, &params_, settings_.frames_per_block),
          "SetNbOfFramesPerBlock");
}

void SampicCrateConfigurator::setTOTMode() {
    Boolean current{};
    check(SAMPIC256CH_GetTOTMeasurementMode(&params_, &current),
          "GetTOTMeasurementMode");

    if ((bool)current == settings_.enable_tot) {
        spdlog::trace("TOTMode already {} → skip", (bool)current);
        return;
    }

    spdlog::trace("TOTMode {} -> {}", (bool)current, settings_.enable_tot);

    check(SAMPIC256CH_SetTOTMeasurementMode(&info_, &params_, settings_.enable_tot),
          "SetTOTMeasurementMode");
}

void SampicCrateConfigurator::setADCBits() {
    int current{};
    check(Get_SystemADCNbOfBits(&params_, &current), "Get_SystemADCNbOfBits");

    if (current == settings_.adc_bits) {
        spdlog::trace("ADC bits already {} → skip", current);
        return;
    }

    spdlog::trace("ADC bits {} -> {}", current, settings_.adc_bits);

    check(Set_SystemADCNbOfBits(&info_, &params_, settings_.adc_bits),
          "Set_SystemADCNbOfBits");
}

void SampicCrateConfigurator::setSmartReadMode() {
    Boolean mode{}; int samples{}, offset{};
    check(SAMPIC256CH_GetSmartReadMode(&params_, &mode, &samples, &offset),
          "GetSmartReadMode");

    if ((bool)mode == settings_.smart_read_mode &&
        samples == settings_.samples_to_read &&
        offset == settings_.read_offset) {
        spdlog::trace("SmartReadMode already mode={}, samples={}, offset={} → skip",
                      (bool)mode, samples, offset);
        return;
    }

    spdlog::trace("SmartReadMode change to mode={}, samples={}, offset={}",
                  settings_.smart_read_mode,
                  settings_.samples_to_read,
                  settings_.read_offset);

    check(SAMPIC256CH_SetSmartReadMode(&info_, &params_,
                                       settings_.smart_read_mode,
                                       settings_.samples_to_read,
                                       settings_.read_offset),
          "SetSmartReadMode");
}

// ------------------- External triggers -------------------
void SampicCrateConfigurator::setExternalTriggerType() {
    ExternalTriggerType_t current{};
    check(SAMPIC256CH_GetExternalTriggerType(&params_, &current),
          "GetExternalTriggerType");

    if (current == settings_.external_trigger_type) {
        spdlog::trace("ExternalTriggerType already {} → skip", int(current));
        return;
    }

    spdlog::trace("ExternalTriggerType {} -> {}", int(current),
                  int(settings_.external_trigger_type));

    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_,
                                             settings_.external_trigger_type),
          "SetExternalTriggerType");
}

void SampicCrateConfigurator::setExternalTriggerLevel() {
    SignalLevel_t current{};
    check(SAMPIC256CH_GetExternalTriggerSigLevel(&params_, &current),
          "GetExternalTriggerSigLevel");

    if (current == settings_.signal_level) {
        spdlog::trace("ExternalTriggerLevel already {} → skip", int(current));
        return;
    }

    spdlog::trace("ExternalTriggerLevel {} -> {}", int(current),
                  int(settings_.signal_level));

    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_,
                                                 settings_.signal_level),
          "SetExternalTriggerSigLevel");
}

void SampicCrateConfigurator::setExternalTriggerEdge() {
    EdgeType_t current{};
    check(SAMPIC256CH_GetExternalTriggerEdge(&params_, &current),
          "GetExternalTriggerEdge");

    if (current == settings_.trigger_edge) {
        spdlog::trace("ExternalTriggerEdge already {} → skip", int(current));
        return;
    }

    spdlog::trace("ExternalTriggerEdge {} -> {}", int(current),
                  int(settings_.trigger_edge));

    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_,
                                             settings_.trigger_edge),
          "SetExternalTriggerEdge");
}

void SampicCrateConfigurator::setMinTriggersPerEvent() {
    unsigned char current{};
    check(SAMPIC256CH_GetMinNbOfTriggersPerEvent(&params_, &current),
          "GetMinNbOfTriggersPerEvent");

    if (current == settings_.triggers_per_event) {
        spdlog::trace("MinTriggersPerEvent already {} → skip", int(current));
        return;
    }

    spdlog::trace("MinTriggersPerEvent {} -> {}", int(current),
                  int(settings_.triggers_per_event));

    check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(&info_, &params_,
                                                 settings_.triggers_per_event),
          "SetMinNbOfTriggersPerEvent");
}

void SampicCrateConfigurator::setLevel3TriggerBuild() {
    Boolean current{};
    TriggerLogicParamStruct l3params{};  // default-initialize a real struct
    check(SAMPIC256CH_GetLevel3TriggerLogic(&params_, &current, &l3params),
          "GetLevel3TriggerLogic");

    if ((bool)current == settings_.level3_trigger_build) {
        spdlog::trace("Level3TriggerBuild already {} → skip", (bool)current);
        return;
    }

    spdlog::trace("Level3TriggerBuild {} -> {}", (bool)current,
                  settings_.level3_trigger_build);

    // Pass the struct back in for Set
    check(SAMPIC256CH_SetLevel3TriggerLogic(&info_, &params_,
                                            settings_.level3_trigger_build,
                                            l3params),
          "SetLevel3TriggerLogic");
}


// ------------------- Gates -------------------
void SampicCrateConfigurator::setPrimitivesGateLength() {
    unsigned char current{};
    check(SAMPIC256CH_GetPrimitivesGateLength(&params_, &current),
          "GetPrimitivesGateLength");

    if (current == settings_.primitives_gate_length) {
        spdlog::trace("PrimitivesGateLength already {} → skip", int(current));
        return;
    }

    spdlog::trace("PrimitivesGateLength {} -> {}",
                  int(current), int(settings_.primitives_gate_length));

    check(SAMPIC256CH_SetPrimitivesGateLength(&info_, &params_,
                                              settings_.primitives_gate_length),
          "SetPrimitivesGateLength");
}

void SampicCrateConfigurator::setLevel2LatencyGateLength() {
    unsigned char current{};
    check(SAMPIC256CH_GetLevel2LatencyGateLength(&params_, &current),
          "GetLevel2LatencyGateLength");

    if (current == settings_.latency_gate_length) {
        spdlog::trace("Level2LatencyGateLength already {} → skip", int(current));
        return;
    }

    spdlog::trace("Level2LatencyGateLength {} -> {}",
                  int(current), int(settings_.latency_gate_length));

    check(SAMPIC256CH_SetLevel2LatencyGateLength(&info_, &params_,
                                                 settings_.latency_gate_length),
          "SetLevel2LatencyGateLength");
}

void SampicCrateConfigurator::setLevel3ExtTrigGate() {
    unsigned char current{};
    check(SAMPIC256CH_GetLevel3ExtTrigGate(&params_, &current),
          "GetLevel3ExtTrigGate");

    if (current == settings_.level3_ext_trig_gate) {
        spdlog::trace("Level3ExtTrigGate already {} → skip", int(current));
        return;
    }

    spdlog::trace("Level3ExtTrigGate {} -> {}",
                  int(current), int(settings_.level3_ext_trig_gate));

    check(SAMPIC256CH_SetLevel3ExtTrigGate(&info_, &params_,
                                           settings_.level3_ext_trig_gate),
          "SetLevel3ExtTrigGate");
}

void SampicCrateConfigurator::setLevel3CoincidenceWithExtGate() {
    Boolean current{};
    check(SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate(&params_, &current),
          "GetLevel3CoincidenceModeWithExtTrigGate");

    if ((bool)current == settings_.level3_coincidence_ext_gate) {
        spdlog::trace("Level3CoincidenceWithExtGate already {} → skip", (bool)current);
        return;
    }

    spdlog::trace("Level3CoincidenceWithExtGate {} -> {}", (bool)current,
                  settings_.level3_coincidence_ext_gate);

    check(SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate(&info_, &params_,
                                                              settings_.level3_coincidence_ext_gate),
          "SetLevel3CoincidenceModeWithExtTrigGate");
}

// ------------------- Pulser -------------------
void SampicCrateConfigurator::setPulser() {
    Boolean en{}; PulserSourceType_t src{}; Boolean sync{}; int period{};
    check(SAMPIC256CH_GetPulserMode(&params_, &en, &src, &sync),
          "GetPulserMode");

    check(SAMPIC256CH_GetAutoPulserPeriod(&params_, &period),
          "GetAutoPulserPeriod");

    if ((bool)en == settings_.pulser_enable &&
        src == settings_.pulser_source &&
        (bool)sync == settings_.pulser_synchronous &&
        period == settings_.pulser_period) {
        spdlog::trace("Pulser already en={}, src={}, sync={}, period={} → skip",
                      (bool)en, int(src), (bool)sync, period);
        return;
    }

    spdlog::trace("Pulser change to en={}, src={}, sync={}, period={}",
                  settings_.pulser_enable,
                  int(settings_.pulser_source),
                  settings_.pulser_synchronous,
                  settings_.pulser_period);

    check(SAMPIC256CH_SetPulserMode(&info_, &params_,
                                    settings_.pulser_enable,
                                    settings_.pulser_source,
                                    settings_.pulser_synchronous),
          "SetPulserMode");

    check(SAMPIC256CH_SetAutoPulserPeriod(&info_, &params_,
                                          settings_.pulser_period),
          "SetAutoPulserPeriod");
}

// ------------------- Sync + corrections -------------------
void SampicCrateConfigurator::setSyncEdge() {
    EdgeType_t current{};
    check(SAMPIC256CH_GetExternalSyncEdge(&params_, &current),
          "GetExternalSyncEdge");

    if (current == settings_.sync_edge) {
        spdlog::trace("SyncEdge already {} → skip", int(current));
        return;
    }

    spdlog::trace("SyncEdge {} -> {}", int(current), int(settings_.sync_edge));

    check(SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, settings_.sync_edge),
          "SetExternalSyncEdge");
}

void SampicCrateConfigurator::setSyncLevel() {
    SignalLevel_t current{};
    check(SAMPIC256CH_GetExternalSyncSigLevel(&params_, &current),
          "GetExternalSyncSigLevel");

    if (current == settings_.sync_level) {
        spdlog::trace("SyncLevel already {} → skip", int(current));
        return;
    }

    spdlog::trace("SyncLevel {} -> {}", int(current), int(settings_.sync_level));

    check(SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, settings_.sync_level),
          "SetExternalSyncSigLevel");
}

void SampicCrateConfigurator::setCorrectionLevels() {
    Boolean adc{}, inl{}, ped{};
    check(SAMPIC256CH_GetCrateCorrectionLevels(&info_, &params_, &adc, &inl, &ped),
          "GetCrateCorrectionLevels");

    if ((bool)adc == settings_.adc_linearity_correction &&
        (bool)inl == settings_.time_inl_correction &&
        (bool)ped == settings_.residual_pedestal_correction) {
        spdlog::trace("CorrectionLevels already adc={}, inl={}, ped={} → skip",
                      (bool)adc, (bool)inl, (bool)ped);
        return;
    }

    spdlog::trace("CorrectionLevels change to adc={}, inl={}, ped={}",
                  settings_.adc_linearity_correction,
                  settings_.time_inl_correction,
                  settings_.residual_pedestal_correction);

    check(SAMPIC256CH_SetCrateCorrectionLevels(&info_, &params_,
                                               settings_.adc_linearity_correction,
                                               settings_.time_inl_correction,
                                               settings_.residual_pedestal_correction),
          "SetCrateCorrectionLevels");
}

// ------------------- Boards descend -------------------
void SampicCrateConfigurator::applyBoards() {
    spdlog::debug("Applying front-end boards...");
    for (auto& [key, febCfg] : settings_.front_end_boards) {
        int febIdx = indexFromKey(key);

        if (!febCfg.enabled) {
            spdlog::info("Skipping FEB '{}' (index={}) — disabled.", key, febIdx);
            continue;
        }

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
