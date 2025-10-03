#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

int main() {
    CrateInfoStruct info{};
    CrateParamStruct params{};
    CrateConnectionParamStruct conn{};

    // ----------------------------
    // Connect to crate
    // ----------------------------
    conn.ConnectionType = UDP_CONNECTION;
    conn.ControlBoardControlType = CTRL_AND_DAQ;
    strncpy(conn.CtrlIpAddress, "192.168.0.4", sizeof(conn.CtrlIpAddress));
    conn.CtrlPort = DEFAULT_UDP_CTRL_PORT;

    auto err = SAMPIC256CH_OpenCrateConnection(conn, &info);
    if (err != SAMPIC256CH_Success) {
        std::cerr << "Failed to open crate connection (err=" << (int)err << ")\n";
        return 1;
    }

    // ----------------------------
    // Load default parameters
    // ----------------------------
    err = SAMPIC256CH_SetDefaultParameters(&info, &params);
    if (err != SAMPIC256CH_Success) {
        std::cerr << "Failed to set default parameters (err=" << (int)err << ")\n";
        return 1;
    }

    // (Optional) load calibration values (we won't emit them; only non-calib pairs requested)
    std::string calib_dir = "resources/calib";
    SAMPIC256CH_LoadAllCalibValuesFromFiles(&info, &params,
                                            const_cast<char*>(calib_dir.c_str()));

    // For brevity in this generator we will read back only:
    const int FE  = 0; // first front-end board
    const int IC  = 0; // first SAMPIC on FE
    const int CH  = 0; // first channel on SAMPIC

    // ============================================================================================
    // CRATE-LEVEL (system) — all params that have both setter/getter (non-calibration)
    // ============================================================================================

    // Sampling Frequency
    int samplingFreq = 0; Boolean useExternalClock = FALSE;
    SAMPIC256CH_GetSamplingFrequency(&params, &samplingFreq, &useExternalClock);

    // ADC bits
    int adcBits = 0;
    Get_SystemADCNbOfBits(&params, &adcBits);

    // Smart read mode
    Boolean smartMode = FALSE; int samplesToRead = 0; int readOffset = 0;
    SAMPIC256CH_GetSmartReadMode(&params, &smartMode, &samplesToRead, &readOffset);

    // TOT measurement mode
    Boolean totEnabled = FALSE;
    SAMPIC256CH_GetTOTMeasurementMode(&params, &totEnabled);

    // Frames per block
    int framesPerBlock = 0;
    SAMPIC256CH_GetNbOfFramesPerBlock(&params, &framesPerBlock);

    // External trigger: type / level / edge
    ExternalTriggerType_t trigType;
    SAMPIC256CH_GetExternalTriggerType(&params, &trigType);
    SignalLevel_t sigLevel;
    SAMPIC256CH_GetExternalTriggerSigLevel(&params, &sigLevel);
    EdgeType_t trigEdge;
    SAMPIC256CH_GetExternalTriggerEdge(&params, &trigEdge);

    // External Sync: edge / level
    EdgeType_t syncEdge; SignalLevel_t syncLevel;
    SAMPIC256CH_GetExternalSyncEdge(&params, &syncEdge);
    SAMPIC256CH_GetExternalSyncSigLevel(&params, &syncLevel);

    // External trigger counter & extTrigID detection
    Boolean enExtTrigCounter = FALSE, enDetectExtTrigID = FALSE;
    SAMPIC256CH_GetExternalTriggerCounterMode(&params, &enExtTrigCounter, &enDetectExtTrigID);

    // Primitives gate length (crate-global)
    unsigned char primitivesGateLen = 0;
    SAMPIC256CH_GetPrimitivesGateLength(&params, &primitivesGateLen);

    // L2 latency gate length (crate-global)
    unsigned char level2LatencyGateLen = 0;
    SAMPIC256CH_GetLevel2LatencyGateLength(&params, &level2LatencyGateLen);

    // L3 Trigger logic (crate-global)
    Boolean level3Build = FALSE;
    TriggerLogicParamStruct l3Logic{};
    SAMPIC256CH_GetLevel3TriggerLogic(&params, &level3Build, &l3Logic);

    // L3 external gate + coincidence (crate-global)
    unsigned char lvl3ExtGate = 0; Boolean lvl3Coinc = FALSE;
    SAMPIC256CH_GetLevel3ExtTrigGate(&params, &lvl3ExtGate);
    SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate(&params, &lvl3Coinc);

    // Trigger-per-event minimum
    unsigned char nbTrigPerEvent = 0;
    SAMPIC256CH_GetMinNbOfTriggersPerEvent(&params, &nbTrigPerEvent);

    // Auto conversion mode (crate-global)
    Boolean autoConv = FALSE;
    SAMPIC256CH_GetAutoConversionMode(&params, &autoConv);

    // Conversion length (crate-global)
    unsigned char convLen = 0;
    SAMPIC256CH_GetConversionLength(&params, &convLen);

    // Pulser (crate-global)
    Boolean pulserEnable = FALSE, pulserSync = FALSE;
    PulserSourceType_t pulserSrc;
    SAMPIC256CH_GetPulserMode(&params, &pulserEnable, &pulserSrc, &pulserSync);
    int pulserPeriod = 0;
    SAMPIC256CH_GetAutoPulserPeriod(&params, &pulserPeriod);

    // Corrections (crate-global flags)
    Boolean adcCorr = FALSE, timeCorr = FALSE, pedCorr = FALSE;
    SAMPIC256CH_GetCrateCorrectionLevels(&info, &params, &adcCorr, &timeCorr, &pedCorr);

    // ============================================================================================
    // FRONT-END BOARD LEVEL (feBoard=FE)
    // ============================================================================================

    // L2 build option (crate-global boolean, but often surfaced per-board in configs)
    Boolean level2Build = FALSE;
    SAMPIC256CH_GetLevel2TriggerBuildOption(&params, &level2Build);

    // FE global trigger option
    FebGlobalTrigger_t feGlobalTrig = FEB_GLOBAL_TRIGGER_IS_L2;
    SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption(&params, FE, &feGlobalTrig);

    // L2 Logic struct + ext trig gate + coincidence for this FE
    TriggerLogicParamStruct l2Logic{};
    SAMPIC256CH_GetLevel2TriggerLogic(&params, FE, &l2Logic);

    unsigned char lvl2ExtGate = 0; Boolean lvl2Coinc = FALSE;
    SAMPIC256CH_GetLevel2ExtTrigGate(&params, FE, &lvl2ExtGate);
    SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate(&params, FE, &lvl2Coinc);

    // ============================================================================================
    // CHIP (SAMPIC) LEVEL (feBoard=FE, sampicIndex=IC)
    // ============================================================================================

    // Baseline reference
    float baselineRef = 0.0f;
    SAMPIC256CH_GetBaselineReference(&params, FE, IC, &baselineRef);

    // External threshold & mode
    float extThresh = 0.0f;
    SAMPIC256CH_GetSampicExternalThreshold(&params, FE, IC, &extThresh);

    Boolean extThreshMode = FALSE;
    SAMPIC256CH_GetSampicExternalThresholdMode(&params, FE, IC, &extThreshMode);

    // TOT config
    SAMPIC_TOTRange_t totRange;
    SAMPIC256CH_GetSampicTOTRange(&params, FE, IC, &totRange);

    Boolean enTotFilter = FALSE, enWideCap = FALSE; float minWidth = 0.0f;
    SAMPIC256CH_GetSampicTOTFilterParams(&params, FE, IC, &enTotFilter, &enWideCap, &minWidth);

    // Post trigger params
    Boolean enPostTrig = FALSE; int postTrigVal = 0;
    SAMPIC256CH_GetSampicPostTrigParams(&params, FE, IC, &enPostTrig, &postTrigVal);

    // Central trigger configuration
    SampicCentralTriggerMode_t ctMode;
    SAMPIC256CH_GetSampicCentralTriggerMode(&params, FE, IC, &ctMode);

    SampicCentralTriggerEffect_t ctEffect;
    SAMPIC256CH_GetSampicCentralTriggerEffect(&params, FE, IC, &ctEffect);

    SAMPIC_CT_PrimitivesMode_t primMode; int primGate = 0;
    SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions(&params, FE, IC, &primMode, &primGate);

    // Trigger option (L1/L2/L3)
    SampicTriggerOption_t trigOption;
    SAMPIC256CH_GetSampicTriggerOption(&params, FE, IC, &trigOption);

    // Enable-trigger mode (use ext trig, open gate, ext gate)
    Boolean useExtAsEnable = FALSE, openGateOnExt = FALSE; unsigned char enTrigExtGate = 0;
    SAMPIC256CH_GetSampicEnableTriggerMode(&params, FE, IC, &useExtAsEnable, &openGateOnExt, &enTrigExtGate);

    // Common dead time
    Boolean commonDeadTime = FALSE;
    SAMPIC256CH_GetSampicCommonDeadTimeMode(&params, FE, IC, &commonDeadTime);

    // Pulser width (per sampic)
    unsigned char pulserWidth = 0;
    SAMPIC256CH_GetSampicPulserWidth(&params, FE, IC, &pulserWidth);

    // Advanced (non-calibration state variables with set/get)
    float adcRampValue = 0.0f;
    SAMPIC256CH_GetSampicADCRampValue(&params, FE, IC, &adcRampValue);

    float vdacDllValue = 0.0f;
    SAMPIC256CH_GetSampicVdacDLLValue(&params, FE, IC, &vdacDllValue);

    float vdacDllContinuity = 0.0f;
    SAMPIC256CH_GetSampicVdacDLLContinuity(&params, FE, IC, &vdacDllContinuity);

    float vdacRosc = 0.0f;
    SAMPIC256CH_GetSampicVdacRosc(&params, FE, IC, &vdacRosc);

    SampicDLLModeType_t dllMode;
    SAMPIC256CH_GetSampicDLLSpeedMode(&params, FE, IC, &dllMode);

    float overflowDac = 0.0f;
    SAMPIC256CH_GetSampicOverflowDacValue(&params, FE, IC, &overflowDac);

    Boolean lvdsLowCurrent = FALSE;
    SAMPIC256CH_GetSampicLvdsLowCurrentMode(&params, FE, IC, &lvdsLowCurrent);

    // ============================================================================================
    // CHANNEL LEVEL (feBoard=FE, sampicIndex=IC, channelIndex=CH)
    // ============================================================================================

    // Channel enable/disable
    Boolean chEnabled = FALSE;
    SAMPIC256CH_GetChannelMode(&params, FE, CH, &chEnabled);

    // Channel trigger mode / threshold / edge
    SAMPIC_ChannelTriggerMode_t chTrigMode;
    SAMPIC256CH_GetSampicChannelTriggerMode(&params, FE, IC, CH, &chTrigMode);

    float intThresh = 0.0f;
    SAMPIC256CH_GetSampicChannelInternalThreshold(&params, FE, IC, CH, &intThresh);

    EdgeType_t chTrigEdge;
    SAMPIC256CH_GetChannelSelfTriggerEdge(&params, FE, IC, CH, &chTrigEdge);

    // Participation to central trigger (CT source)
    Boolean enCT = FALSE;
    SAMPIC256CH_GetSampicChannelSourceForCT(&params, FE, IC, CH, &enCT);

    // Pulse mode on channel
    Boolean pulseMode = FALSE;
    SAMPIC256CH_GetSampicChannelPulseMode(&params, FE, IC, CH, &pulseMode);

    // ----------------------------
    // Write header
    // ----------------------------
    std::filesystem::create_directories("generated");
    std::ofstream out("generated/sampic_config_generated.h");
    if (!out) { std::cerr << "Failed to open output file\n"; return 1; }

    out << "#ifndef SAMPIC_CONFIG_H\n#define SAMPIC_CONFIG_H\n\n";
    out << "#include <string>\n#include <cstdint>\n#include <map>\n\n";
    out << "extern \"C\" {\n#include <SAMPIC_256Ch_Type.h>\n}\n\n";

    // ============================================================================================
    // Channel struct
    // ============================================================================================
    out << "// ==============================\n";
    out << "// Channel (lowest level)\n";
    out << "// ==============================\n";
    out << "struct SampicChannelConfig {\n";
    out << "  bool enabled = " << (chEnabled ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetChannelMode | Getter: SAMPIC256CH_GetChannelMode\n";
    out << "  SAMPIC_ChannelTriggerMode_t trigger_mode = static_cast<SAMPIC_ChannelTriggerMode_t>(" << chTrigMode << ")"
        << "; // Setter: SAMPIC256CH_SetSampicChannelTriggerMode | Getter: SAMPIC256CH_GetSampicChannelTriggerMode\n";
    out << "  float internal_threshold = " << intThresh
        << "; // Setter: SAMPIC256CH_SetSampicChannelInternalThreshold | Getter: SAMPIC256CH_GetSampicChannelInternalThreshold\n";
    out << "  EdgeType_t trigger_edge = static_cast<EdgeType_t>(" << chTrigEdge << ")"
        << "; // Setter: SAMPIC256CH_SetChannelSelflTriggerEdge | Getter: SAMPIC256CH_GetChannelSelfTriggerEdge\n";
    out << "  bool enable_for_central_trigger = " << (enCT ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicChannelSourceForCT | Getter: SAMPIC256CH_GetSampicChannelSourceForCT\n";
    out << "  bool pulse_mode = " << (pulseMode ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicChannelPulseMode | Getter: SAMPIC256CH_GetSampicChannelPulseMode\n";
    out << "};\n\n";
    out << "using SampicChannelSettings = std::map<std::string, SampicChannelConfig>;\n\n";

    // ============================================================================================
    // Chip struct
    // ============================================================================================
    out << "// ==============================\n";
    out << "// Chip (contains channels)\n";
    out << "// ==============================\n";
    out << "struct SampicChipConfig {\n";
    out << "  bool enabled = true; // Local enable flag (no direct setter/getter)\n\n";

    out << "  float baseline_reference = " << baselineRef
        << "; // Setter: SAMPIC256CH_SetBaselineReference | Getter: SAMPIC256CH_GetBaselineReference\n";
    out << "  float external_threshold = " << extThresh
        << "; // Setter: SAMPIC256CH_SetSampicExternalThreshold | Getter: SAMPIC256CH_GetSampicExternalThreshold\n";
    out << "  bool external_threshold_mode = " << (extThreshMode ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicExternalThresholdMode | Getter: SAMPIC256CH_GetSampicExternalThresholdMode\n";
    out << "  SAMPIC_TOTRange_t tot_range = static_cast<SAMPIC_TOTRange_t>(" << totRange << ")"
        << "; // Setter: SAMPIC256CH_SetSampicTOTRange | Getter: SAMPIC256CH_GetSampicTOTRange\n";
    out << "  bool tot_filter_enable = " << (enTotFilter ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams\n";
    out << "  bool tot_wide_cap = " << (enWideCap ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams\n";
    out << "  float tot_min_width_ns = " << minWidth
        << "; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams\n";
    out << "  bool enable_post_trigger = " << (enPostTrig ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicPostTrigParams | Getter: SAMPIC256CH_GetSampicPostTrigParams\n";
    out << "  int post_trigger_value = " << postTrigVal
        << "; // Setter: SAMPIC256CH_SetSampicPostTrigParams | Getter: SAMPIC256CH_GetSampicPostTrigParams\n";

    out << "  SampicCentralTriggerMode_t central_trigger_mode = static_cast<SampicCentralTriggerMode_t>(" << ctMode << ")"
        << "; // Setter: SAMPIC256CH_SetSampicCentralTriggerMode | Getter: SAMPIC256CH_GetSampicCentralTriggerMode\n";
    out << "  SampicCentralTriggerEffect_t central_trigger_effect = static_cast<SampicCentralTriggerEffect_t>(" << ctEffect << ")"
        << "; // Setter: SAMPIC256CH_SetSampicCentralTriggerEffect | Getter: SAMPIC256CH_GetSampicCentralTriggerEffect\n";
    out << "  SAMPIC_CT_PrimitivesMode_t primitives_mode = static_cast<SAMPIC_CT_PrimitivesMode_t>(" << primMode << ")"
        << "; // Setter: SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions | Getter: SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions\n";
    out << "  int primitives_gate_length = " << primGate
        << "; // Setter: SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions | Getter: SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions\n";

    out << "  SampicTriggerOption_t trigger_option = static_cast<SampicTriggerOption_t>(" << trigOption << ")"
        << "; // Setter: SAMPIC256CH_SetSampicTriggerOption | Getter: SAMPIC256CH_GetSampicTriggerOption\n";

    out << "  bool enable_trigger_use_external = " << (useExtAsEnable ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode\n";
    out << "  bool enable_trigger_open_gate_on_ext = " << (openGateOnExt ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode\n";
    out << "  unsigned char enable_trigger_ext_gate = " << (int)enTrigExtGate
        << "; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode\n";

    out << "  bool common_dead_time = " << (commonDeadTime ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicCommonDeadTimeMode | Getter: SAMPIC256CH_GetSampicCommonDeadTimeMode\n";

    out << "  unsigned char pulser_width = " << (int)pulserWidth
        << "; // Setter: SAMPIC256CH_SetSampicPulserWidth | Getter: SAMPIC256CH_GetSampicPulserWidth\n";

    out << "  float adc_ramp_value = " << adcRampValue
        << "; // Setter: SAMPIC256CH_SetSampicADCRampValue | Getter: SAMPIC256CH_GetSampicADCRampValue\n";
    out << "  float vdac_dll_value = " << vdacDllValue
        << "; // Setter: SAMPIC256CH_SetSampicVdacDLLValue | Getter: SAMPIC256CH_GetSampicVdacDLLValue\n";
    out << "  float vdac_dll_continuity = " << vdacDllContinuity
        << "; // Setter: SAMPIC256CH_SetSampicVdacDLLContinuity | Getter: SAMPIC256CH_GetSampicVdacDLLContinuity\n";
    out << "  float vdac_rosc = " << vdacRosc
        << "; // Setter: SAMPIC256CH_SetSampicVdacRosc | Getter: SAMPIC256CH_GetSampicVdacRosc\n";
    out << "  SampicDLLModeType_t dll_speed_mode = static_cast<SampicDLLModeType_t>(" << dllMode << ")"
        << "; // Setter: SAMPIC256CH_SetSampicDLLSpeedMode | Getter: SAMPIC256CH_GetSampicDLLSpeedMode\n";
    out << "  float overflow_dac_value = " << overflowDac
        << "; // Setter: SAMPIC256CH_SetSampicOverflowDacValue | Getter: SAMPIC256CH_GetSampicOverflowDacValue\n";
    out << "  bool lvds_low_current_mode = " << (lvdsLowCurrent ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSampicLvdsLowCurrentMode | Getter: SAMPIC256CH_GetSampicLvdsLowCurrentMode\n\n";

    out << "  SampicChannelSettings channels {\n";
    out << "    {\"channel0\", {}}, {\"channel1\", {}}, {\"channel2\", {}}, {\"channel3\", {}},\n";
    out << "    {\"channel4\", {}}, {\"channel5\", {}}, {\"channel6\", {}}, {\"channel7\", {}},\n";
    out << "    {\"channel8\", {}}, {\"channel9\", {}}, {\"channel10\", {}}, {\"channel11\", {}},\n";
    out << "    {\"channel12\", {}}, {\"channel13\", {}}, {\"channel14\", {}}, {\"channel15\", {}}\n";
    out << "  };\n";
    out << "};\n\n";
    out << "using SampicChipSettings = std::map<std::string, SampicChipConfig>;\n\n";

    // ============================================================================================
    // FrontEnd struct
    // ============================================================================================
    out << "// ==============================\n";
    out << "// Front-End Board (contains chips)\n";
    out << "// ==============================\n";
    out << "struct SampicFrontEndConfig {\n";
    out << "  bool enabled = true; // Local enable flag (no direct setter/getter)\n\n";
    out << "  FebGlobalTrigger_t global_trigger_option = static_cast<FebGlobalTrigger_t>(" << feGlobalTrig << ")"
        << "; // Setter: SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption | Getter: SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption\n";
    out << "  bool level2_trigger_build = " << (level2Build ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetLevel2TriggerBuildOption | Getter: SAMPIC256CH_GetLevel2TriggerBuildOption\n";
    out << "  unsigned char level2_ext_trig_gate = " << (int)lvl2ExtGate
        << "; // Setter: SAMPIC256CH_SetLevel2ExtTrigGate | Getter: SAMPIC256CH_GetLevel2ExtTrigGate\n";
    out << "  bool level2_coincidence_ext_gate = " << (lvl2Coinc ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate | Getter: SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate\n";
    out << "  SampicChipSettings sampics {\n";
    out << "    {\"sampic0\", {}}, {\"sampic1\", {}}, {\"sampic2\", {}}, {\"sampic3\", {}}\n";
    out << "  };\n";
    out << "};\n\n";
    out << "using SampicFrontEndSettings = std::map<std::string, SampicFrontEndConfig>;\n\n";

    // ============================================================================================
    // Crate (system) struct
    // ============================================================================================
    out << "// ==============================\n";
    out << "// Crate (top-level system)\n";
    out << "// ==============================\n";
    out << "struct SampicSystemSettings {\n";
    out << "  // Init parameters (not a set/get pair; carried here for ODB convenience)\n";
    out << "  std::string ip_address = \"192.168.0.4\"; // via SAMPIC256CH_OpenCrateConnection\n";
    out << "  int port = " << DEFAULT_UDP_CTRL_PORT << ";\n";
    out << "  ConnectionType_t connection_type = static_cast<ConnectionType_t>(UDP_CONNECTION);\n";
    out << "  ControlType_t control_type = static_cast<ControlType_t>(CTRL_AND_DAQ);\n";
    out << "  std::string calibration_directory = \"resources/calib\";\n";

    out << "  // Acquisition \n";
    out << "  int sampling_frequency_mhz = " << samplingFreq
        << "; // Setter: SAMPIC256CH_SetSamplingFrequency | Getter: SAMPIC256CH_GetSamplingFrequency\n";
    out << "  bool use_external_clock = " << (useExternalClock ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSamplingFrequency | Getter: SAMPIC256CH_GetSamplingFrequency\n";
    out << "  int adc_bits = " << adcBits
        << "; // Setter: Set_SystemADCNbOfBits | Getter: Get_SystemADCNbOfBits\n";
    out << "  bool smart_read_mode = " << (smartMode ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode\n";
    out << "  int read_offset = " << readOffset
        << "; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode\n";
    out << "  int samples_to_read = " << samplesToRead
        << "; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode\n";
    out << "  bool enable_tot = " << (totEnabled ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetTOTMeasurementMode | Getter: SAMPIC256CH_GetTOTMeasurementMode\n";
    out << "  int frames_per_block = " << framesPerBlock
        << "; // Setter: SAMPIC256CH_SetNbOfFramesPerBlock | Getter: SAMPIC256CH_GetNbOfFramesPerBlock\n";
    out << "  bool auto_conversion = " << (autoConv ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetAutoConversionMode | Getter: SAMPIC256CH_GetAutoConversionMode\n";
    out << "  unsigned char conversion_length = " << (int)convLen
        << "; // Setter: SAMPIC256CH_SetConversionLength | Getter: SAMPIC256CH_GetConversionLength\n\n";

    out << "  // Triggers \n";
    out << "  ExternalTriggerType_t external_trigger_type = static_cast<ExternalTriggerType_t>(" << trigType << ")"
        << "; // Setter: SAMPIC256CH_SetExternalTriggerType | Getter: SAMPIC256CH_GetExternalTriggerType\n";
    out << "  SignalLevel_t signal_level = static_cast<SignalLevel_t>(" << sigLevel << ")"
        << "; // Setter: SAMPIC256CH_SetExternalTriggerSigLevel | Getter: SAMPIC256CH_GetExternalTriggerSigLevel\n";
    out << "  EdgeType_t trigger_edge = static_cast<EdgeType_t>(" << trigEdge << ")"
        << "; // Setter: SAMPIC256CH_SetExternalTriggerEdge | Getter: SAMPIC256CH_GetExternalTriggerEdge\n";
    out << "  unsigned char triggers_per_event = " << (int)nbTrigPerEvent
        << "; // Setter: SAMPIC256CH_SetMinNbOfTriggersPerEvent | Getter: SAMPIC256CH_GetMinNbOfTriggersPerEvent\n";
    out << "  bool level3_trigger_build = " << (level3Build ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetLevel3TriggerLogic | Getter: SAMPIC256CH_GetLevel3TriggerLogic\n";
    out << "  unsigned char level3_ext_trig_gate = " << (int)lvl3ExtGate
        << "; // Setter: SAMPIC256CH_SetLevel3ExtTrigGate | Getter: SAMPIC256CH_GetLevel3ExtTrigGate\n";
    out << "  bool level3_coincidence_ext_gate = " << (lvl3Coinc ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate | Getter: SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate\n";
    out << "  unsigned char primitives_gate_length = " << (int)primitivesGateLen
        << "; // Setter: SAMPIC256CH_SetPrimitivesGateLength | Getter: SAMPIC256CH_GetPrimitivesGateLength\n";
    out << "  unsigned char latency_gate_length = " << (int)level2LatencyGateLen
        << "; // Setter: SAMPIC256CH_SetLevel2LatencyGateLength | Getter: SAMPIC256CH_GetLevel2LatencyGateLength\n\n";

    out << "  // External trigger counter & timestamping\n";
    out << "  bool enable_external_trigger_counter = " << (enExtTrigCounter ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetExternalTriggerCounterMode | Getter: SAMPIC256CH_GetExternalTriggerCounterMode\n";
    out << "  bool enable_detect_ext_trigger_id = " << (enDetectExtTrigID ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetExternalTriggerCounterMode | Getter: SAMPIC256CH_GetExternalTriggerCounterMode\n\n";

    out << "  // Pulser \n";
    out << "  bool pulser_enable = " << (pulserEnable ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode\n";
    out << "  PulserSourceType_t pulser_source = static_cast<PulserSourceType_t>(" << pulserSrc << ")"
        << "; // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode\n";
    out << "  bool pulser_synchronous = " << (pulserSync ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode\n";
    out << "  int pulser_period = " << pulserPeriod
        << "; // Setter: SAMPIC256CH_SetAutoPulserPeriod | Getter: SAMPIC256CH_GetAutoPulserPeriod\n\n";

    out << "  // Sync\n";
    out << "  EdgeType_t sync_edge = static_cast<EdgeType_t>(" << syncEdge << ")"
        << "; // Setter: SAMPIC256CH_SetExternalSyncEdge | Getter: SAMPIC256CH_GetExternalSyncEdge\n";
    out << "  SignalLevel_t sync_level = static_cast<SignalLevel_t>(" << syncLevel << ")"
        << "; // Setter: SAMPIC256CH_SetExternalSyncSigLevel | Getter: SAMPIC256CH_GetExternalSyncSigLevel\n\n";

    out << "  // Corrections \n";
    out << "  bool adc_linearity_correction = " << (adcCorr ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels\n";
    out << "  bool time_inl_correction = " << (timeCorr ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels\n";
    out << "  bool residual_pedestal_correction = " << (pedCorr ? "true" : "false")
        << "; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels\n\n";

    out << "  // Hierarchy (maps for ODB compatibility)\n";
    out << "  SampicFrontEndSettings front_end_boards {\n";
    out << "    {\"feb0\", {}}, {\"feb1\", {}}, {\"feb2\", {}}, {\"feb3\", {}}\n";
    out << "  };\n";
    out << "};\n\n";


    out << "#endif // SAMPIC_CONFIG_H\n";
    std::cout << "Generated: generated/sampic_config_generated.h\n";
    return 0;
}
