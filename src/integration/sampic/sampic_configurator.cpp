#include "integration/sampic/sampic_configurator.h"
#include <spdlog/spdlog.h>
#include <cstring>

SampicConfigurator::SampicConfigurator(CrateInfoStruct& info,
                                       CrateParamStruct& params,
                                       void*& eventBuffer,
                                       ML_Frame*& mlFrames)
    : info_(info), params_(params),
      eventBuffer_(eventBuffer), mlFrames_(mlFrames) {}

void SampicConfigurator::initializeHardware(const SampicSystemSettings& settings) {
    SAMPIC256CH_ErrCode err;

    spdlog::info("Initializing SAMPIC hardware (IP: {}, Port: {})",
                 settings.ip_address, settings.port);

    CrateConnectionParamStruct conn{};
    conn.ConnectionType = settings.connection_type;
    conn.ControlBoardControlType = settings.control_type;
    strncpy(conn.CtrlIpAddress, settings.ip_address.c_str(),
            sizeof(conn.CtrlIpAddress) - 1);
    conn.CtrlIpAddress[sizeof(conn.CtrlIpAddress) - 1] = '\0';
    conn.CtrlPort = settings.port;

    err = SAMPIC256CH_OpenCrateConnection(conn, &info_);
    check(err, "OpenCrateConnection");

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    check(err, "SetDefaultParameters");

    char calib_dir[MAX_PATHNAME_LENGTH];
    strncpy(calib_dir, settings.calibration.calibration_directory.c_str(),
            sizeof(calib_dir) - 1);
    calib_dir[sizeof(calib_dir) - 1] = '\0';

    err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, calib_dir);
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("Calibration files not loaded (directory: '{}')",
                     settings.calibration.calibration_directory);
    }

    err = SAMPIC256CH_AllocateEventMemory(&eventBuffer_, &mlFrames_);
    check(err, "AllocateEventMemory");

    spdlog::info("SAMPIC hardware initialization complete.");
}

void SampicConfigurator::applySettings(const SampicSystemSettings& settings) {
    spdlog::info("Applying SAMPIC settings...");

    applyAcquisition(settings.acquisition);
    applyTrigger(settings.trigger);
    applyPulser(settings.pulser);
    applySync(settings.synchronization);
    applyCalibration(settings.calibration);
    applyFrontEnds(settings.front_end_boards);

    spdlog::info("SAMPIC settings applied.");
}

// ------------------- Acquisition -------------------
void SampicConfigurator::applyAcquisition(const SampicAcquisitionSettings& acq) {
    check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_,
                                           acq.sampling_frequency_mhz,
                                           acq.use_external_clock ? 1 : 0),
          "SetSamplingFrequency");

    check(SAMPIC256CH_SetNbOfFramesPerBlock(&info_, &params_, acq.frames_per_block),
          "SetNbOfFramesPerBlock");

    check(SAMPIC256CH_SetTOTMeasurementMode(&info_, &params_, acq.enable_tot ? 1 : 0),
          "SetTOTMeasurementMode");

    check(Set_SystemADCNbOfBits(&info_, &params_, acq.adc_bits),
          "Set_SystemADCNbOfBits");

    check(SAMPIC256CH_SetSmartReadMode(&info_, &params_,
                                       acq.smart_read_mode ? 1 : 0,
                                       acq.samples_to_read,
                                       acq.read_offset),
          "SetSmartReadMode");
}

// ------------------- Trigger -------------------
void SampicConfigurator::applyTrigger(const SampicTriggerSettings& trig) {
    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_, trig.external_trigger_type),
          "SetExternalTriggerType");

    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, trig.signal_level),
          "SetExternalTriggerSigLevel");

    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, trig.trigger_edge),
          "SetExternalTriggerEdge");

    check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(&info_, &params_, trig.triggers_per_event),
          "SetMinNbOfTriggersPerEvent");

    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_,
                                                  trig.level2_trigger_build ? 1 : 0),
          "SetLevel2TriggerBuildOption");

    if (trig.level3_trigger_build) {
        TriggerLogicParamStruct dummy{}; // TODO: fill from config
        check(SAMPIC256CH_SetLevel3TriggerLogic(&info_, &params_, 1, dummy),
              "SetLevel3TriggerLogic");
    }
}

// ------------------- Pulser -------------------
void SampicConfigurator::applyPulser(const SampicPulserSettings& pulser) {
    check(SAMPIC256CH_SetPulserMode(&info_, &params_,
                                    pulser.enable ? 1 : 0,
                                    pulser.source,
                                    pulser.synchronous ? 1 : 0),
          "SetPulserMode");

    check(SAMPIC256CH_SetAutoPulserPeriod(&info_, &params_, pulser.period),
          "SetAutoPulserPeriod");

    check(SAMPIC256CH_SetSampicPulserWidth(&info_, &params_, 0, 0, pulser.width),
          "SetSampicPulserWidth");
}

// ------------------- Sync -------------------
void SampicConfigurator::applySync(const SampicSyncSettings& sync) {
    check(SAMPIC256CH_SetCrateSycnhronisationMode(&info_, &params_,
                                                  sync.sync_mode ? 1 : 0,
                                                  sync.master_mode ? 1 : 0,
                                                  sync.coincidence_mode ? 1 : 0),
          "SetCrateSycnhronisationMode");

    check(SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, sync.sync_edge),
          "SetExternalSyncEdge");

    check(SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, sync.sync_level),
          "SetExternalSyncSigLevel");
}

// ------------------- Calibration -------------------
void SampicConfigurator::applyCalibration(const SampicCalibrationSettings& calib) {
    check(SAMPIC256CH_SetCrateCorrectionLevels(&info_, &params_,
                                               calib.adc_linearity_correction ? 1 : 0,
                                               calib.time_inl_correction ? 1 : 0,
                                               calib.residual_pedestal_correction ? 1 : 0),
          "SetCrateCorrectionLevels");
}

// ------------------- Hierarchy -------------------
void SampicConfigurator::applyFrontEnds(const SampicFrontEndSettings& febs) {
    for (const auto& [febKey, febCfg] : febs) {
        int febIdx = indexFromKey(febKey);
        applyFrontEnd(febIdx, febCfg);
    }
}

void SampicConfigurator::applyFrontEnd(int febIdx, const SampicFrontEndConfig& cfg) {
    check(SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(&info_, &params_,
                                                          febIdx, cfg.global_trigger_option),
          "SetFrontEndBoardGlobalTriggerOption");

    for (const auto& [chipKey, chipCfg] : cfg.sampics) {
        int chipIdx = indexFromKey(chipKey);
        applyChip(febIdx, chipIdx, chipCfg);
    }
}

void SampicConfigurator::applyChip(int febIdx, int chipIdx, const SampicChipConfig& cfg) {
    check(SAMPIC256CH_SetBaselineReference(&info_, &params_, febIdx, chipIdx,
                                           cfg.baseline_reference),
          "SetBaselineReference");

    check(SAMPIC256CH_SetSampicExternalThreshold(&info_, &params_, febIdx, chipIdx,
                                                 cfg.external_threshold),
          "SetSampicExternalThreshold");

    check(SAMPIC256CH_SetSampicTOTRange(&info_, &params_, febIdx, chipIdx, cfg.tot_range),
          "SetSampicTOTRange");

    check(SAMPIC256CH_SetSampicPostTrigParams(&info_, &params_, febIdx, chipIdx,
                                              cfg.enable_post_trigger ? 1 : 0,
                                              cfg.post_trigger_value),
          "SetSampicPostTrigParams");

    check(SAMPIC256CH_SetSampicCentralTriggerMode(&info_, &params_, febIdx, chipIdx,
                                                  cfg.central_trigger_mode),
          "SetSampicCentralTriggerMode");

    check(SAMPIC256CH_SetSampicCentralTriggerEffect(&info_, &params_, febIdx, chipIdx,
                                                    cfg.central_trigger_effect),
          "SetSampicCentralTriggerEffect");

    check(SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions(&info_, &params_,
                                                               febIdx, chipIdx,
                                                               cfg.primitives_mode,
                                                               cfg.primitives_gate_length),
          "SetSampicCentralTriggerPrimitivesOptions");

    for (const auto& [chKey, chCfg] : cfg.channels) {
        int chIdx = indexFromKey(chKey);
        applyChannel(febIdx, chipIdx, chIdx, chCfg);
    }
}

void SampicConfigurator::applyChannel(int febIdx, int chipIdx, int chIdx,
                                      const SampicChannelConfig& cfg) {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, febIdx, chIdx,
                                     cfg.enabled ? 1 : 0),
          "SetChannelMode");

    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_,
                                                  febIdx, chipIdx, chIdx,
                                                  cfg.trigger_mode),
          "SetSampicChannelTriggerMode");

    check(SAMPIC256CH_SetSampicChannelInternalThreshold(&info_, &params_,
                                                        febIdx, chipIdx, chIdx,
                                                        cfg.internal_threshold),
          "SetSampicChannelInternalThreshold");

    check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_,
                                                 febIdx, chipIdx, chIdx,
                                                 cfg.trigger_edge),
          "SetChannelSelflTriggerEdge");

    check(SAMPIC256CH_SetSampicExternalThresholdMode(&info_, &params_,
                                                     febIdx, chipIdx,
                                                     cfg.external_threshold_mode ? 1 : 0),
          "SetSampicExternalThresholdMode");

    check(SAMPIC256CH_SetSampicChannelSourceForCT(&info_, &params_,
                                                  febIdx, chipIdx, chIdx,
                                                  cfg.enable_for_central_trigger ? 1 : 0),
          "SetSampicChannelSourceForCT");

    check(SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_,
                                                febIdx, chipIdx, chIdx,
                                                cfg.pulse_mode ? 1 : 0),
          "SetSampicChannelPulseMode");
}

// ------------------- Utility -------------------
void SampicConfigurator::check(SAMPIC256CH_ErrCode code, const std::string& what) {
    if (code != SAMPIC256CH_Success) {
        spdlog::error("SAMPIC error in {} (code={})", what, static_cast<int>(code));
        throw std::runtime_error("SAMPIC error in " + what +
                                 " (code " + std::to_string(code) + ")");
    }
}

int SampicConfigurator::indexFromKey(const std::string& key) {
    size_t pos = key.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        throw std::runtime_error("Invalid config key: " + key);
    }
    return std::stoi(key.substr(pos + 1));
}
