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
  bool                         enabled  = true;                          // Global enable for channel
  SAMPIC_ChannelTriggerMode_t  trigger_mode = SAMPIC_CHANNEL_EXT_TRIGGER_MODE; // SAMPIC256CH_SetSampicChannelTriggerMode
  float                        internal_threshold = 0.0;                 // SAMPIC256CH_SetSampicChannelInternalThreshold
  EdgeType_t                   trigger_edge = RISING_EDGE;               // SAMPIC256CH_SetChannelSelflTriggerEdge
  bool                         external_threshold_mode = false;          // SAMPIC256CH_SetSampicExternalThresholdMode
  bool                         enable_for_central_trigger = true;        // SAMPIC256CH_SetSampicChannelSourceForCT
  bool                         pulse_mode = false;                       // SAMPIC256CH_SetSampicChannelPulseMode
};

using SampicChannelSettings = std::map<std::string, SampicChannelConfig>;

// ==============================
// Chip (contains channels)
// ==============================
struct SampicChipConfig {
  bool                          enabled = true;                          // Global enable for chip

  float                         baseline_reference = DEFAULT_SAMPIC_BASELINE; // SAMPIC256CH_SetBaselineReference
  float                         external_threshold = 0.5;                    // SAMPIC256CH_SetSampicExternalThreshold

  SAMPIC_TOTRange_t             tot_range = SAMPIC_TOT_RANGE_MAX_25_NS;       // SAMPIC256CH_SetSampicTOTRange
  bool                          enable_post_trigger = true;                  // SAMPIC256CH_SetSampicPostTrigParams
  int                           post_trigger_value = 7;                       // SAMPIC256CH_SetSampicPostTrigParams

  SampicCentralTriggerMode_t    central_trigger_mode   = CENTRAL_OR;          // SAMPIC256CH_SetSampicCentralTriggerMode
  SampicCentralTriggerEffect_t  central_trigger_effect = TRIG_CHANNEL_ONLY_IF_PARTICIPATING_TO_CT; // SAMPIC256CH_SetSampicCentralTriggerEffect
  SAMPIC_CT_PrimitivesMode_t    primitives_mode        = RAW_PRIMITIVES_FOR_CT; // SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions
  int                           primitives_gate_length = DEFAULT_PRIMITIVES_GATE_LENGTH; // SAMPIC256CH_SetSampicCentralTriggerPrimitivesOptions

  bool   tot_filter_enable = false;   // SAMPIC256CH_SetSampicTOTFilterParams
  bool   tot_wide_cap      = false;   // SAMPIC256CH_SetSampicTOTFilterParams
  float  tot_min_width_ns  = 0.0;     // SAMPIC256CH_SetSampicTOTFilterParams

  SampicChannelSettings channels {
    {"channel0", {}}, {"channel1", {}}, {"channel2", {}}, {"channel3", {}},
    {"channel4", {}}, {"channel5", {}}, {"channel6", {}}, {"channel7", {}},
    {"channel8", {}}, {"channel9", {}}, {"channel10", {}}, {"channel11", {}},
    {"channel12", {}}, {"channel13", {}}, {"channel14", {}}, {"channel15", {}}
  };
};

using SampicChipSettings = std::map<std::string, SampicChipConfig>;

// ==============================
// Board (contains chips)
// ==============================
struct SampicFrontEndConfig {
  bool enabled = true;   // Global enable for this FE board

  FebGlobalTrigger_t global_trigger_option = FEB_GLOBAL_TRIGGER_IS_L2;       // SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption

  unsigned char     level2_ext_trig_gate  = DEFAULT_EXT_TRIG_GATE;           // SAMPIC256CH_SetLevel2ExtTrigGate
  bool              level2_coincidence_ext_gate = false;                     // SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate

  SampicChipSettings sampics {
    {"sampic0", {}}, {"sampic1", {}}, {"sampic2", {}}, {"sampic3", {}}
  };
};

using SampicFrontEndSettings = std::map<std::string, SampicFrontEndConfig>;

// ==============================
// Crate (top-level system)
// ==============================
struct SampicSystemSettings {
  std::string    ip_address     = "192.168.0.4"; //DEFAULT_CTRL_IP_ADDRESS
  int            port           = DEFAULT_UDP_CTRL_PORT;   // UDP control port
  ConnectionType_t connection_type = UDP_CONNECTION;       // SAMPIC256CH_OpenCrateConnection
  ControlType_t    control_type    = CTRL_AND_DAQ;         // SAMPIC256CH_OpenCrateConnection

  // Acquisition
  int  sampling_frequency_mhz = DEFAULT_FREQ_ECH;   // SAMPIC256CH_SetSamplingFrequency
  bool use_external_clock     = false;
  int  frames_per_block       = MAX_NB_OF_FRAMES_PER_BLOCK;  // SAMPIC256CH_SetNbOfFramesPerBlock
  bool enable_tot             = false;              // SAMPIC256CH_SetTOTMeasurementMode
  int  adc_bits               = DEFAULT_ADC_NB_OF_BITS; // Set_SystemADCNbOfBits
  bool smart_read_mode        = false;              // SAMPIC256CH_SetSmartReadMode
  int  read_offset            = 0;
  int  samples_to_read        = MAX_NB_OF_SAMPLES;

  // Trigger
  ExternalTriggerType_t external_trigger_type = SOFTWARE;      // SAMPIC256CH_SetExternalTriggerType
  SignalLevel_t         signal_level          = TTL_SIG;       // SAMPIC256CH_SetExternalTriggerSigLevel
  EdgeType_t            trigger_edge          = RISING_EDGE;   // SAMPIC256CH_SetExternalTriggerEdge
  std::uint8_t          triggers_per_event    = DEFAULT_NB_OF_TRIGGERS_PER_TRIGGER_EVENT; // SAMPIC256CH_SetMinNbOfTriggersPerEvent
  bool                  level2_trigger_build  = false;         // SAMPIC256CH_SetLevel2TriggerBuildOption
  bool                  level3_trigger_build  = false;         // SAMPIC256CH_SetLevel3TriggerLogic
  SampicTriggerOption_t trigger_option        = SAMPIC_TRIGGER_IS_L1; // SAMPIC256CH_SetSampicTriggerOption
  unsigned char         level3_ext_trig_gate  = DEFAULT_EXT_TRIG_GATE; // SAMPIC256CH_SetLevel3ExtTrigGate
  bool                  level3_coincidence_ext_gate = false;   // SAMPIC256CH_SetLevel3CoincidenceModeWithExtTrigGate
  unsigned char         primitives_gate_length = DEFAULT_PRIMITIVES_GATE_LENGTH; // SAMPIC256CH_SetPrimitivesGateLength
  unsigned char         latency_gate_length    = DEFAULT_LEVEL_2_LATENCY_GATE;   // SAMPIC256CH_SetLevel2LatencyGateLength

  // Pulser
  bool               pulser_enable      = false;           // SAMPIC256CH_SetPulserMode
  PulserSourceType_t pulser_source      = PULSER_SRC_IS_SOFT_CMD;
  bool               pulser_synchronous = true;
  int                pulser_period      = DEFAULT_AUTO_PULSE_PERIOD;   // SAMPIC256CH_SetAutoPulserPeriod
  unsigned char      pulser_width       = DEFAULT_PULSER_WIDTH;        // SAMPIC256CH_SetSampicPulserWidth

  // Sync + corrections
  bool        sync_mode        = false;           // SAMPIC256CH_SetCrateSycnhronisationMode
  bool        master_mode      = false;
  bool        coincidence_mode = false;
  EdgeType_t  sync_edge        = RISING_EDGE;     // SAMPIC256CH_SetExternalSyncEdge
  SignalLevel_t sync_level     = TTL_SIG;         // SAMPIC256CH_SetExternalSyncSigLevel

  bool        adc_linearity_correction   = false; // SAMPIC256CH_SetCrateCorrectionLevels
  bool        time_inl_correction        = false;
  bool        residual_pedestal_correction = false;
  std::string calibration_directory      = ".";   // SAMPIC256CH_LoadAllCalibValuesFromFiles

  SampicFrontEndSettings    front_end_boards {
    {"feb0", {}}, {"feb1", {}}, {"feb2", {}}, {"feb3", {}}
  };

  bool verbose = false; // for logging verbosity
};

#endif // SAMPIC_CONFIG_H
