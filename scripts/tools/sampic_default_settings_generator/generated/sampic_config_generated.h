#ifndef SAMPIC_CONFIG_H
#define SAMPIC_CONFIG_H

#include <string>
#include <cstdint>
#include <map>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

// ==============================
// Channel (lowest level)
// ==============================
struct SampicChannelConfig {
  bool enabled = true; // Setter: SAMPIC256CH_SetChannelMode | Getter: SAMPIC256CH_GetChannelMode
  SAMPIC_ChannelTriggerMode_t trigger_mode = static_cast<SAMPIC_ChannelTriggerMode_t>(3); // Setter: SAMPIC256CH_SetSampicChannelTriggerMode | Getter: SAMPIC256CH_GetSampicChannelTriggerMode
  float internal_threshold = 0.1; // Setter: SAMPIC256CH_SetSampicChannelInternalThreshold | Getter: SAMPIC256CH_GetSampicChannelInternalThreshold
  EdgeType_t trigger_edge = static_cast<EdgeType_t>(0); // Setter: SAMPIC256CH_SetChannelSelflTriggerEdge | Getter: SAMPIC256CH_GetChannelSelfTriggerEdge
  bool enable_for_central_trigger = true; // Setter: SAMPIC256CH_SetSampicChannelSourceForCT | Getter: SAMPIC256CH_GetSampicChannelSourceForCT
  bool pulse_mode = false; // Setter: SAMPIC256CH_SetSampicChannelPulseMode | Getter: SAMPIC256CH_GetSampicChannelPulseMode
};

using SampicChannelSettings = std::map<std::string, SampicChannelConfig>;

// ==============================
// Chip (contains channels)
// ==============================
struct SampicChipConfig {
  bool enabled = true; // Local enable flag (no direct setter/getter)

  float baseline_reference = 0.5; // Setter: SAMPIC256CH_SetBaselineReference | Getter: SAMPIC256CH_GetBaselineReference
  float external_threshold = 0.1; // Setter: SAMPIC256CH_SetSampicExternalThreshold | Getter: SAMPIC256CH_GetSampicExternalThreshold
  bool external_threshold_mode = false; // Setter: SAMPIC256CH_SetSampicExternalThresholdMode | Getter: SAMPIC256CH_GetSampicExternalThresholdMode
  SAMPIC_TOTRange_t tot_range = static_cast<SAMPIC_TOTRange_t>(2); // Setter: SAMPIC256CH_SetSampicTOTRange | Getter: SAMPIC256CH_GetSampicTOTRange
  bool tot_filter_enable = false; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams
  bool tot_wide_cap = false; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams
  float tot_min_width_ns = 10; // Setter: SAMPIC256CH_SetSampicTOTFilterParams | Getter: SAMPIC256CH_GetSampicTOTFilterParams
  bool enable_post_trigger = false; // Setter: SAMPIC256CH_SetSampicPostTrigParams | Getter: SAMPIC256CH_GetSampicPostTrigParams
  int post_trigger_value = 0; // Setter: SAMPIC256CH_SetSampicPostTrigParams | Getter: SAMPIC256CH_GetSampicPostTrigParams
  SampicCentralTriggerMode_t central_trigger_mode = static_cast<SampicCentralTriggerMode_t>(0); // Setter: SAMPIC256CH_SetSampicCentralTriggerMode | Getter: SAMPIC256CH_GetSampicCentralTriggerMode
  SampicCentralTriggerEffect_t central_trigger_effect = static_cast<SampicCentralTriggerEffect_t>(0); // Setter: SAMPIC256CH_SetSampicCentralTriggerEffect | Getter: SAMPIC256CH_GetSampicCentralTriggerEffect
  SAMPIC_CT_PrimitivesMode_t primitives_mode = static_cast<SAMPIC_CT_PrimitivesMode_t>(1); // Setter: SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions | Getter: SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions
  int primitives_gate_length = 5; // Setter: SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions | Getter: SAMPIC256CH_GetSampicCentralTriggerPrimitivesOptions
  SampicTriggerOption_t trigger_option = static_cast<SampicTriggerOption_t>(0); // Setter: SAMPIC256CH_SetSampicTriggerOption | Getter: SAMPIC256CH_GetSampicTriggerOption
  bool enable_trigger_use_external = false; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode
  bool enable_trigger_open_gate_on_ext = false; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode
  unsigned char enable_trigger_ext_gate = 8; // Setter: SAMPIC256CH_SetSampicEnableTriggerMode | Getter: SAMPIC256CH_GetSampicEnableTriggerMode
  bool common_dead_time = false; // Setter: SAMPIC256CH_SetSampicCommonDeadTimeMode | Getter: SAMPIC256CH_GetSampicCommonDeadTimeMode
  unsigned char pulser_width = 10; // Setter: SAMPIC256CH_SetSampicPulserWidth | Getter: SAMPIC256CH_GetSampicPulserWidth
  float adc_ramp_value = 0.045; // Setter: SAMPIC256CH_SetSampicADCRampValue | Getter: SAMPIC256CH_GetSampicADCRampValue
  float vdac_dll_value = 1.1; // Setter: SAMPIC256CH_SetSampicVdacDLLValue | Getter: SAMPIC256CH_GetSampicVdacDLLValue
  float vdac_dll_continuity = 1.1; // Setter: SAMPIC256CH_SetSampicVdacDLLContinuity | Getter: SAMPIC256CH_GetSampicVdacDLLContinuity
  float vdac_rosc = 1; // Setter: SAMPIC256CH_SetSampicVdacRosc | Getter: SAMPIC256CH_GetSampicVdacRosc
  SampicDLLModeType_t dll_speed_mode = static_cast<SampicDLLModeType_t>(3); // Setter: SAMPIC256CH_SetSampicDLLSpeedMode | Getter: SAMPIC256CH_GetSampicDLLSpeedMode
  float overflow_dac_value = 1; // Setter: SAMPIC256CH_SetSampicOverflowDacValue | Getter: SAMPIC256CH_GetSampicOverflowDacValue
  bool lvds_low_current_mode = true; // Setter: SAMPIC256CH_SetSampicLvdsLowCurrentMode | Getter: SAMPIC256CH_GetSampicLvdsLowCurrentMode

  SampicChannelSettings channels {
    {"channel0", {}}, {"channel1", {}}, {"channel2", {}}, {"channel3", {}},
    {"channel4", {}}, {"channel5", {}}, {"channel6", {}}, {"channel7", {}},
    {"channel8", {}}, {"channel9", {}}, {"channel10", {}}, {"channel11", {}},
    {"channel12", {}}, {"channel13", {}}, {"channel14", {}}, {"channel15", {}}
  };
};

using SampicChipSettings = std::map<std::string, SampicChipConfig>;

// ==============================
// Front-End Board (contains chips)
// ==============================
struct SampicFrontEndConfig {
  bool enabled = true; // Local enable flag (no direct setter/getter)

  FebGlobalTrigger_t global_trigger_option = static_cast<FebGlobalTrigger_t>(0); // Setter: SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption | Getter: SAMPIC256CH_GetFrontEndBoardGlobalTriggerOption
  bool level2_trigger_build = false; // Setter: SAMPIC256CH_SetLevel2TriggerBuildOption | Getter: SAMPIC256CH_GetLevel2TriggerBuildOption
  unsigned char level2_ext_trig_gate = 8; // Setter: SAMPIC256CH_SetLevel2ExtTrigGate | Getter: SAMPIC256CH_GetLevel2ExtTrigGate
  bool level2_coincidence_ext_gate = false; // Setter: SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate | Getter: SAMPIC256CH_GetLevel2CoincidenceModeWithExtTrigGate
  SampicChipSettings sampics {
    {"sampic0", {}}, {"sampic1", {}}, {"sampic2", {}}, {"sampic3", {}}
  };
};

using SampicFrontEndSettings = std::map<std::string, SampicFrontEndConfig>;

// ==============================
// Crate (top-level system)
// ==============================
struct SampicSystemSettings {
  // Init parameters (not a set/get pair; carried here for ODB convenience)
  std::string ip_address = "192.168.0.4"; // via SAMPIC256CH_OpenCrateConnection
  int port = 27015;
  ConnectionType_t connection_type = static_cast<ConnectionType_t>(UDP_CONNECTION);
  ControlType_t control_type = static_cast<ControlType_t>(CTRL_AND_DAQ);
  std::string calibration_directory = "resources/calib";
  // Acquisition 
  int sampling_frequency_mhz = 6400; // Setter: SAMPIC256CH_SetSamplingFrequency | Getter: SAMPIC256CH_GetSamplingFrequency
  bool use_external_clock = false; // Setter: SAMPIC256CH_SetSamplingFrequency | Getter: SAMPIC256CH_GetSamplingFrequency
  int adc_bits = 11; // Setter: Set_SystemADCNbOfBits | Getter: Get_SystemADCNbOfBits
  bool smart_read_mode = false; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode
  int read_offset = 0; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode
  int samples_to_read = 64; // Setter: SAMPIC256CH_SetSmartReadMode | Getter: SAMPIC256CH_GetSmartReadMode
  bool enable_tot = false; // Setter: SAMPIC256CH_SetTOTMeasurementMode | Getter: SAMPIC256CH_GetTOTMeasurementMode
  int frames_per_block = 1; // Setter: SAMPIC256CH_SetNbOfFramesPerBlock | Getter: SAMPIC256CH_GetNbOfFramesPerBlock
  bool auto_conversion = true; // Setter: SAMPIC256CH_SetAutoConversionMode | Getter: SAMPIC256CH_GetAutoConversionMode
  unsigned char conversion_length = 250; // Setter: SAMPIC256CH_SetConversionLength | Getter: SAMPIC256CH_GetConversionLength

  // Triggers 
  ExternalTriggerType_t external_trigger_type = static_cast<ExternalTriggerType_t>(0); // Setter: SAMPIC256CH_SetExternalTriggerType | Getter: SAMPIC256CH_GetExternalTriggerType
  SignalLevel_t signal_level = static_cast<SignalLevel_t>(0); // Setter: SAMPIC256CH_SetExternalTriggerSigLevel | Getter: SAMPIC256CH_GetExternalTriggerSigLevel
  EdgeType_t trigger_edge = static_cast<EdgeType_t>(0); // Setter: SAMPIC256CH_SetExternalTriggerEdge | Getter: SAMPIC256CH_GetExternalTriggerEdge
  unsigned char triggers_per_event = 127; // Setter: SAMPIC256CH_SetMinNbOfTriggersPerEvent | Getter: SAMPIC256CH_GetMinNbOfTriggersPerEvent
  bool level3_trigger_build = false; // Setter: SAMPIC256CH_SetLevel3TriggerLogic | Getter: SAMPIC256CH_GetLevel3TriggerLogic
  unsigned char level3_ext_trig_gate = 8; // Setter: SAMPIC256CH_SetLevel3ExtTrigGate | Getter: SAMPIC256CH_GetLevel3ExtTrigGate
  bool level3_coincidence_ext_gate = false; // Setter: SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate | Getter: SAMPIC256CH_GetLevel3CoincidenceModeWithExtTrigGate
  unsigned char primitives_gate_length = 5; // Setter: SAMPIC256CH_SetPrimitivesGateLength | Getter: SAMPIC256CH_GetPrimitivesGateLength
  unsigned char latency_gate_length = 3; // Setter: SAMPIC256CH_SetLevel2LatencyGateLength | Getter: SAMPIC256CH_GetLevel2LatencyGateLength

  // External trigger counter & timestamping
  bool enable_external_trigger_counter = false; // Setter: SAMPIC256CH_SetExternalTriggerCounterMode | Getter: SAMPIC256CH_GetExternalTriggerCounterMode
  bool enable_detect_ext_trigger_id = false; // Setter: SAMPIC256CH_SetExternalTriggerCounterMode | Getter: SAMPIC256CH_GetExternalTriggerCounterMode

  // Pulser 
  bool pulser_enable = false; // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode
  PulserSourceType_t pulser_source = static_cast<PulserSourceType_t>(0); // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode
  bool pulser_synchronous = true; // Setter: SAMPIC256CH_SetPulserMode | Getter: SAMPIC256CH_GetPulserMode
  int pulser_period = 10; // Setter: SAMPIC256CH_SetAutoPulserPeriod | Getter: SAMPIC256CH_GetAutoPulserPeriod

  // Sync
  EdgeType_t sync_edge = static_cast<EdgeType_t>(0); // Setter: SAMPIC256CH_SetExternalSyncEdge | Getter: SAMPIC256CH_GetExternalSyncEdge
  SignalLevel_t sync_level = static_cast<SignalLevel_t>(0); // Setter: SAMPIC256CH_SetExternalSyncSigLevel | Getter: SAMPIC256CH_GetExternalSyncSigLevel

  // Corrections 
  bool adc_linearity_correction = false; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels
  bool time_inl_correction = false; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels
  bool residual_pedestal_correction = false; // Setter: SAMPIC256CH_SetCrateCorrectionLevels | Getter: SAMPIC256CH_GetCrateCorrectionLevels

  // Hierarchy (maps for ODB compatibility)
  SampicFrontEndSettings front_end_boards {
    {"feb0", {}}, {"feb1", {}}, {"feb2", {}}, {"feb3", {}}
  };
};

#endif // SAMPIC_CONFIG_H
