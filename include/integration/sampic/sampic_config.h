#ifndef SAMPIC_CONFIG_H
#define SAMPIC_CONFIG_H

#include <string>
#include <cstdint>
#include <map>
#include <rfl.hpp>

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

// ==============================
// Acquisition (crate/common layer)
// ==============================
struct SampicAcquisitionSettings {
  int sampling_frequency_mhz = DEFAULT_FREQ_ECH;   // SetSamplingFrequency
  bool use_external_clock = false;

  int frames_per_block = 1;                        // SetNbOfFramesPerBlock
  bool enable_tot = false;                         // SetTOTMeasurementMode
  int adc_bits = DEFAULT_ADC_NB_OF_BITS;           // Set_SystemADCNbOfBits

  bool smart_read_mode = false;                    // SetSmartReadMode
  int read_offset = 0;                             // SetSmartReadMode
  int samples_to_read = MAX_NB_OF_SAMPLES;         // SetSmartReadMode
};

// ==============================
// Trigger (crate-level + external)
// ==============================
struct SampicTriggerSettings {
  ExternalTriggerType_t external_trigger_type = SOFTWARE;
  SignalLevel_t signal_level = NIM_SIG;
  EdgeType_t trigger_edge = RISING_EDGE;

  std::uint8_t triggers_per_event = 1;             // SetMinNbOfTriggersPerEvent
  bool level2_trigger_build = false;               // SetLevel2TriggerBuildOption
  bool level3_trigger_build = false;               // SetLevel3TriggerLogic
};

// ==============================
// Pulser (crate + FEB-level)
// ==============================
struct SampicPulserSettings {
  bool enable = false;                             // SetPulserMode
  PulserSourceType_t source = PULSER_SRC_IS_SOFT_CMD;
  bool synchronous = true;

  int period = DEFAULT_AUTO_PULSE_PERIOD;          // SetAutoPulserPeriod
  unsigned char width = DEFAULT_PULSER_WIDTH;      // SetSampicPulserWidth
};

// ==============================
// Synchronization (crate-level)
// ==============================
struct SampicSyncSettings {
  bool sync_mode = false;                          // SetCrateSycnhronisationMode
  bool master_mode = false;                        // SetCrateSycnhronisationMode
  bool coincidence_mode = false;                   // SetCrateSycnhronisationMode
  EdgeType_t sync_edge = RISING_EDGE;              // SetExternalSyncEdge
  SignalLevel_t sync_level = NIM_SIG;              // SetExternalSyncSigLevel
};

// ==============================
// Calibration
// ==============================
struct SampicCalibrationSettings {
  bool adc_linearity_correction = false;           // SetCrateCorrectionLevels
  bool time_inl_correction = false;                // SetCrateCorrectionLevels
  bool residual_pedestal_correction = false;       // SetCrateCorrectionLevels
  std::string calibration_directory = "";          // LoadAllCalibValuesFromFiles
};

// ==============================
// Channel
// ==============================
struct SampicChannelConfig {
  bool enabled = true;                                          // SetChannelMode
  SAMPIC_ChannelTriggerMode_t trigger_mode =
      SAMPIC_CHANNEL_EXT_TRIGGER_MODE;                          // SetSampicChannelTriggerMode
  float internal_threshold = 0.1;                               // SetSampicChannelInternalThreshold
  EdgeType_t trigger_edge = RISING_EDGE;                        // SetChannelSelflTriggerEdge
  bool external_threshold_mode = false;                         // SetSampicExternalThresholdMode
  bool enable_for_central_trigger = true;                       // SetSampicChannelSourceForCT
  bool pulse_mode = false;                                      // SetSampicChannelPulseMode
};

using SampicChannelSettings = std::map<std::string, SampicChannelConfig>;

// ==============================
// SAMPIC chip (per FE-FPGA)
// ==============================
struct SampicChipConfig {
  float baseline_reference = DEFAULT_SAMPIC_BASELINE;           // SetBaselineReference
  float external_threshold = 0.5;                               // SetSampicExternalThreshold

  SAMPIC_TOTRange_t tot_range = SAMPIC_TOT_RANGE_MAX_25_NS;     // SetSampicTOTRange
  bool enable_post_trigger = false;                             // SetSampicPostTrigParams
  int post_trigger_value = 0;                                   // SetSampicPostTrigParams

  SampicCentralTriggerMode_t central_trigger_mode = CENTRAL_OR; // SetSampicCentralTriggerMode
  SampicCentralTriggerEffect_t central_trigger_effect =
      TRIG_CHANNEL_ONLY_IF_PARTICIPATING_TO_CT;                 // SetSampicCentralTriggerEffect
  SAMPIC_CT_PrimitivesMode_t primitives_mode = RAW_PRIMITIVES_FOR_CT;
  int primitives_gate_length = DEFAULT_PRIMITIVES_GATE_LENGTH;  // SetSampicCentralTriggerPrimitivesOptions

  SampicChannelSettings channels {
    {"channel0", {}}, {"channel1", {}}, {"channel2", {}}, {"channel3", {}},
    {"channel4", {}}, {"channel5", {}}, {"channel6", {}}, {"channel7", {}},
    {"channel8", {}}, {"channel9", {}}, {"channel10", {}}, {"channel11", {}},
    {"channel12", {}}, {"channel13", {}}, {"channel14", {}}, {"channel15", {}}
  };
};

using SampicChipSettings = std::map<std::string, SampicChipConfig>;

// ==============================
// Front-End Board
// ==============================
struct SampicFrontEndConfig {
  bool trigger_enable = true;                                   // future use
  FebGlobalTrigger_t global_trigger_option = FEB_GLOBAL_TRIGGER_IS_L2; // SetFrontEndBoardGlobalTriggerOption

  SampicChipSettings sampics {
    {"sampic0", {}}, {"sampic1", {}}, {"sampic2", {}}, {"sampic3", {}}
  };
};

using SampicFrontEndSettings = std::map<std::string, SampicFrontEndConfig>;

// ==============================
// System (crate, connection, runtime)
// ==============================
struct SampicSystemSettings {
  std::string ip_address = "192.168.0.4";
  int port = DEFAULT_UDP_CTRL_PORT;
  ConnectionType_t connection_type = UDP_CONNECTION;
  ControlType_t control_type = CTRL_AND_DAQ;

  SampicAcquisitionSettings   acquisition;
  SampicTriggerSettings       trigger;
  SampicPulserSettings        pulser;
  SampicSyncSettings          synchronization;
  SampicCalibrationSettings   calibration;

  SampicFrontEndSettings      front_end_boards {
    {"feb0", {}}, {"feb1", {}}, {"feb2", {}}, {"feb3", {}}
  };

  bool verbose = false;
};

#endif // SAMPIC_CONFIG_H
