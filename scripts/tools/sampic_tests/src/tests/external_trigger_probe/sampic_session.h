#pragma once

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

#include "runtime_and_options.h"
#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/probe_fatal_error.h"

using sampic::double_pulse::ConnectionConfig;
using sampic::double_pulse::ExternalTriggerConfig;
using sampic::double_pulse::ReadoutConfig;
using sampic::double_pulse::StartRetryConfig;

class SimpleSession {
 public:
  SimpleSession(
      const ConnectionConfig& conn,
      const ExternalTriggerConfig& trig,
      bool use_self_trigger_channels)
      : conn_opts_(conn),
        trig_opts_(trig),
        use_self_trigger_channels_(use_self_trigger_channels) {
    try {
      initialise_connection();
      configure_base();
      allocate_event_memory();
      configure_defaults();
    } catch (...) {
      // A throwing constructor does not run this class's destructor. Release
      // the vendor connection explicitly so the next scan retry starts clean.
      if (event_buffer_ || ml_frames_) {
        SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
      }
      if (connected_) {
        SAMPIC256CH_CloseCrateConnection(&info_);
        connected_ = false;
      }
      throw;
    }
  }

  ~SimpleSession() {
    try {
      stop_run();
    } catch (const std::exception& error) {
      std::cerr << "Warning: SAMPIC cleanup failed: " << error.what() << "\n";
    }
    if (event_buffer_ || ml_frames_) {
      SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
    }
    if (connected_) {
      SAMPIC256CH_CloseCrateConnection(&info_);
    }
  }

  void set_sampling_rate(int rate_mhz) {
    check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_, rate_mhz,
                                           conn_opts_.use_external_clock),
          "SetSamplingFrequency");
    int readback_mhz = 0;
    Boolean readback_external_clock = FALSE;
    check(SAMPIC256CH_GetSamplingFrequency(
              &params_, &readback_mhz, &readback_external_clock),
          "GetSamplingFrequency");
    const bool expected_external_clock = conn_opts_.use_external_clock != FALSE;
    const bool applied_external_clock = readback_external_clock != FALSE;
    if (readback_mhz != rate_mhz ||
        applied_external_clock != expected_external_clock) {
      throw sampic::tests::ProbeFatalError(
          "Sampling-frequency readback mismatch: requested " +
          std::to_string(rate_mhz) + " MHz with external_clock=" +
          (expected_external_clock ? "true" : "false") + ", got " +
          std::to_string(readback_mhz) + " MHz with external_clock=" +
          (applied_external_clock ? "true" : "false"));
    }
    sampling_frequency_requested_mhz_ = rate_mhz;
    sampling_frequency_readback_mhz_ = readback_mhz;
    sampling_frequency_external_clock_ = applied_external_clock;
    std::cout << "SAMPIC sampling frequency verified: requested="
              << rate_mhz << " MHz, readback=" << readback_mhz
              << " MHz, external_clock="
              << (applied_external_clock ? "true" : "false") << std::endl;
  }

  int sampling_frequency_requested_mhz() const {
    return sampling_frequency_requested_mhz_;
  }

  int sampling_frequency_readback_mhz() const {
    return sampling_frequency_readback_mhz_;
  }

  bool sampling_frequency_external_clock() const {
    return sampling_frequency_external_clock_;
  }

  void set_packetization(int frames_per_block, int triggers_per_event) {
    check(SAMPIC256CH_SetNbOfFramesPerBlock(
              &info_, &params_, frames_per_block),
          "SetNbOfFramesPerBlock");
    check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(
              &info_,
              &params_,
              static_cast<unsigned char>(triggers_per_event)),
          "SetMinNbOfTriggersPerEvent");
  }

  void enable_channels(int board_index, const std::vector<int>& channels) {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, ALL_CHANNELs, FALSE),
          "DisableBoardChannels");
    for (int ch : channels) {
      check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, ch, TRUE),
            "EnableChannel");
    }
  }

  void enable_all_channels() {
    check(SAMPIC256CH_SetChannelMode(
              &info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, TRUE),
          "EnableAllChannels");
  }

  void enable_l2_external_gate(
      bool all_channels,
      int selected_board,
      const std::vector<int>& selected_channels,
      int primitive_gate_clocks,
      int latency_gate_clocks,
      int external_gate_clocks) {
    check(SAMPIC256CH_SetSampicChannelSourceForCT(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              FALSE),
          "DisableAllCentralTriggerSources");
    if (all_channels) {
      check(SAMPIC256CH_SetSampicChannelSourceForCT(
                &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
                TRUE),
            "EnableAllCentralTriggerSources");
    } else {
      for (const int channel : selected_channels) {
        const int sampic = channel / NB_OF_CHANNELS_IN_SAMPIC;
        const int sampic_channel = channel % NB_OF_CHANNELS_IN_SAMPIC;
        check(SAMPIC256CH_SetSampicChannelSourceForCT(
                  &info_, &params_, selected_board, sampic, sampic_channel,
                  TRUE),
              "EnableCentralTriggerSource");
      }
    }

    check(SAMPIC256CH_SetSampicCentralTriggerMode(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, CENTRAL_OR),
          "SetCentralTriggerMode(OR)");
    check(SAMPIC256CH_SetSampicCentralTriggerEffect(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
              TRIG_CHANNEL_ONLY_IF_PARTICIPATING_TO_CT),
          "SetCentralTriggerEffect(ParticipatingChannels)");
    check(SAMPIC256CH_SetSampicTriggerOption(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
              SAMPIC_TRISSER_IS_FEB_GT),
          "SetSampicTriggerOption(FEB_GT)");
    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, TRUE),
          "SetLevel2TriggerBuildOption");
    check(SAMPIC256CH_SetPrimitivesGateLength(
              &info_, &params_,
              static_cast<unsigned char>(primitive_gate_clocks)),
          "SetPrimitivesGateLength");
    check(SAMPIC256CH_SetLevel2LatencyGateLength(
              &info_, &params_,
              static_cast<unsigned char>(latency_gate_clocks)),
          "SetLevel2LatencyGateLength");

    TriggerLogicParamStruct logic{};
    logic.SelInput0 = 0;
    logic.SelInput1 = 1;
    logic.SelInput2 = 2;
    logic.SelInput3 = 3;
    logic.Layer1TriggerLogic0 = LOGIC_OR;
    logic.Layer1TriggerLogic1 = LOGIC_OR;
    logic.Layer1TriggerLogic2 = LOGIC_OR;
    logic.Layer2TriggerLogic0 = LOGIC_OR;
    logic.Layer2TriggerLogic1 = LOGIC_OR;
    logic.Layer3TriggerLogic = LOGIC_OR;

    for (int board = 0; board < info_.NbOfFeBoards; ++board) {
      check(SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(
                &info_, &params_, board, FEB_GLOBAL_TRIGGER_IS_L2),
            "SetFrontEndBoardGlobalTriggerOption(L2)");
      check(SAMPIC256CH_SetLevel2TriggerLogic(
                &info_, &params_, board, logic),
            "SetLevel2TriggerLogic(OR)");
      check(SAMPIC256CH_SetLevel2ExtTrigGate(
                &info_, &params_, board,
                static_cast<unsigned char>(external_gate_clocks)),
            "SetLevel2ExtTrigGate");
      check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(
                &info_, &params_, board, TRUE),
            "SetLevel2CoincidenceModeWithExtTrigGate");
    }

    std::cout
        << "L2 external hardware gate: ENABLED on " << info_.NbOfFeBoards
        << " FEB(s)\n"
        << "  trigger path: channel self-trigger primitives -> FEB L2 OR "
           "-> external-gate coincidence -> SAMPIC readout\n"
        << "  central-trigger sources: "
        << (all_channels ? "all enabled channels on all FEBs"
                         : "selected channels on the selected FEB")
        << "\n"
        << "  primitive gate: " << primitive_gate_clocks << " clocks ("
        << primitive_gate_clocks * 10 << " ns)\n"
        << "  latency gate:   " << latency_gate_clocks << " clocks ("
        << latency_gate_clocks * 10 << " ns)\n"
        << "  external gate:  " << external_gate_clocks << " clocks ("
        << external_gate_clocks * 10 << " ns)\n";
  }

  void enable_plain_self_trigger() {
    check(SAMPIC256CH_SetSampicChannelTriggerMode(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
          "SetSampicChannelTriggerMode(SelfTrigger)");
    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, FALSE),
          "DisableLevel2TriggerBuild");
    for (int board = 0; board < info_.NbOfFeBoards; ++board) {
      check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(
                &info_, &params_, board, FALSE),
            "DisableLevel2CoincidenceModeWithExtTrigGate");
    }
    check(SAMPIC256CH_SetSampicChannelSourceForCT(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              FALSE),
          "DisableAllCentralTriggerSources");
    check(SAMPIC256CH_SetSampicTriggerOption(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
              SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption(L1)");
    check(SAMPIC256CH_SetExternalTriggerCounterMode(
              &info_, &params_, TRUE, TRUE),
          "SetExternalTriggerCounterMode(ReferenceOnly)");

    std::cout
        << "Acquisition scheme: plain channel self-trigger\n"
        << "  L2 trigger building: DISABLED\n"
        << "  external-gate coincidence: DISABLED\n"
        << "  external trigger counter remains enabled for offline comparison\n";
  }

  void enable_plain_external_trigger() {
    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, FALSE),
          "DisableLevel2TriggerBuild");
    for (int board = 0; board < info_.NbOfFeBoards; ++board) {
      check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(
                &info_, &params_, board, FALSE),
            "DisableLevel2CoincidenceModeWithExtTrigGate");
    }
    check(SAMPIC256CH_SetSampicChannelSourceForCT(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              FALSE),
          "DisableAllCentralTriggerSources");
    check(SAMPIC256CH_SetSampicChannelTriggerMode(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              SAMPIC_CHANNEL_EXT_TRIGGER_MODE),
          "SetSampicChannelTriggerMode(ExternalTrigger)");
    check(SAMPIC256CH_SetSampicTriggerOption(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
              SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption(L1)");
    check(SAMPIC256CH_SetExternalTriggerCounterMode(
              &info_, &params_, TRUE, TRUE),
          "SetExternalTriggerCounterMode");

    std::cout
        << "Acquisition scheme: plain external trigger\n"
        << "  all enabled channels are read out for every external trigger\n"
        << "  L2 trigger building: DISABLED\n"
        << "  external-gate coincidence: DISABLED\n";
  }

  bool start_run(const StartRetryConfig& retry_cfg) {
    for (int attempt = 1; attempt <= retry_cfg.max_attempts; ++attempt) {
      const auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
      if (err == SAMPIC256CH_Success) {
        run_active_ = true;
        return true;
      }
      const double sleep_seconds =
          retry_cfg.initial_delay_s * std::pow(retry_cfg.backoff, attempt - 1);
      std::this_thread::sleep_for(std::chrono::duration<double>(sleep_seconds));
    }
    return false;
  }

  void stop_run() {
    if (!run_active_) return;
    const auto error = SAMPIC256CH_StopRun(&info_, &params_);
    run_active_ = false;
    if (error != SAMPIC256CH_Success) {
      throw sampic::tests::ProbeFatalError(
          "SAMPIC256CH_StopRun failed (code " +
          std::to_string(static_cast<int>(error)) + ")");
    }
  }

  bool read_event(const ReadoutConfig& readout,
                  EventStruct& event,
                  int& hits_out,
                  int& frames_out,
                  int& bytes_out,
                  bool report_timeout = true,
                  ReadEventTiming* timing = nullptr) {
    if (timing) *timing = {};
    auto timed_call = [](auto&& callback) {
      const auto start = std::chrono::steady_clock::now();
      callback();
      return std::chrono::duration<double, std::micro>(
                 std::chrono::steady_clock::now() - start)
          .count();
    };

    double prepare_event_us = 0.0;
    prepare_event_us += timed_call(
        [&]() { SAMPIC256CH_PrepareEvent(&info_, &params_); });
    if (timing) timing->prepare_event_us = prepare_event_us;
    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int loop_counter = 0;
    hits_out = 0;
    bytes_out = 0;

    while (err != SAMPIC256CH_Success) {
      const double read_buffer_us = timed_call([&]() {
        err = SAMPIC256CH_ReadEventBuffer(
            &info_, 0, event_buffer_, ml_frames_, &nframes);
      });
      if (timing) {
        ++timing->read_buffer_calls;
        timing->read_buffer_us += read_buffer_us;
      }
      if (err == SAMPIC256CH_Success) {
        const double decode_event_us = timed_call([&]() {
          err = SAMPIC256CH_DecodeEvent(
              &info_, &params_, ml_frames_, &event, nframes, &hits_out);
        });
        if (timing) timing->decode_event_us += decode_event_us;
      }
      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        std::cerr << "Acquisition/Decode error code " << static_cast<int>(err) << "\n";
        return false;
      }
      if (err != SAMPIC256CH_Success) {
        ++loop_counter;
        if ((loop_counter % readout.prepare_interval) == 0) {
          prepare_event_us += timed_call(
              [&]() { SAMPIC256CH_PrepareEvent(&info_, &params_); });
          if (timing) timing->prepare_event_us = prepare_event_us;
        }
        if (readout.max_loops > 0 && loop_counter > readout.max_loops) {
          if (report_timeout) {
            std::cerr << "Read loop exceeded max attempts\n";
          }
          return false;
        }
        if (readout.retry_sleep_us > 0) {
          if (timing) {
            timing->requested_retry_sleep_us += readout.retry_sleep_us;
          }
          std::this_thread::sleep_for(std::chrono::microseconds(readout.retry_sleep_us));
        }
      }
    }

    if (timing) timing->prepare_event_us = prepare_event_us;

    frames_out = nframes;
    for (int i = 0; i < nframes; ++i) {
      const int size = ml_frames_[i].data_size;
      if (size > 0) bytes_out += size;
    }
    return true;
  }

  bool read_raw_event(const ReadoutConfig& readout,
                      RawVendorEvent& raw_event,
                      bool report_timeout = true) {
    raw_event.vendor_timing = {};
    auto timed_call = [](auto&& callback) {
      const auto start = std::chrono::steady_clock::now();
      callback();
      return std::chrono::duration<double, std::micro>(
                 std::chrono::steady_clock::now() - start)
          .count();
    };

    raw_event.vendor_timing.prepare_event_us += timed_call(
        [&]() { SAMPIC256CH_PrepareEvent(&info_, &params_); });
    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int frame_count = 0;
    int loop_counter = 0;
    while (err != SAMPIC256CH_Success) {
      raw_event.vendor_timing.read_buffer_us += timed_call([&]() {
        err = SAMPIC256CH_ReadEventBuffer(
            &info_, 0, event_buffer_, ml_frames_, &frame_count);
      });
      ++raw_event.vendor_timing.read_buffer_calls;
      if (err == SAMPIC256CH_AcquisitionError ||
          err == SAMPIC256CH_ErrInvalidEvent) {
        std::cerr << "Acquisition error code " << static_cast<int>(err)
                  << "\n";
        return false;
      }
      if (err != SAMPIC256CH_Success) {
        ++loop_counter;
        if ((loop_counter % readout.prepare_interval) == 0) {
          raw_event.vendor_timing.prepare_event_us += timed_call(
              [&]() { SAMPIC256CH_PrepareEvent(&info_, &params_); });
        }
        if (readout.max_loops > 0 && loop_counter > readout.max_loops) {
          if (report_timeout) {
            std::cerr << "Read loop exceeded max attempts\n";
          }
          return false;
        }
        if (readout.retry_sleep_us > 0) {
          raw_event.vendor_timing.requested_retry_sleep_us +=
              readout.retry_sleep_us;
          std::this_thread::sleep_for(
              std::chrono::microseconds(readout.retry_sleep_us));
        }
      }
    }

    raw_event.copy_from(ml_frames_, frame_count);
    return true;
  }

  void copy_decoder_context(CrateInfoStruct& info,
                            CrateParamStruct& params) const {
    info = info_;
    params = params_;
  }

  static bool decode_raw_event(CrateInfoStruct& info,
                               CrateParamStruct& params,
                               RawVendorEvent& raw_event,
                               EventStruct& event,
                               int& hits_out) {
    const auto start = std::chrono::steady_clock::now();
    auto error = SAMPIC256CH_DecodeEvent(
        &info,
        &params,
        raw_event.frames.data(),
        &event,
        static_cast<int>(raw_event.frames.size()),
        &hits_out);
    raw_event.vendor_timing.decode_event_us =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start)
            .count();
    return error == SAMPIC256CH_Success;
  }

 private:
  void initialise_connection() {
    std::memset(&conn_, 0, sizeof(conn_));
    conn_.ConnectionType = UDP_CONNECTION;
    conn_.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(conn_.CtrlIpAddress, sizeof(conn_.CtrlIpAddress), "%s",
                  conn_opts_.ip.c_str());
    conn_.CtrlPort = conn_opts_.port;
    const auto error = SAMPIC256CH_OpenCrateConnection(conn_, &info_);
    if (error != SAMPIC256CH_Success) {
      throw std::runtime_error(
          "OpenCrateConnection failed (code " +
          std::to_string(static_cast<int>(error)) + ")");
    }
    connected_ = true;
    std::cout << "Connected to crate. FEBs=" << info_.NbOfFeBoards << "\n";
  }

  void configure_base() {
    check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");
    if (conn_opts_.load_calibration) {
      namespace fs = std::filesystem;
      fs::path calib{conn_opts_.calibration_dir};
      if (!calib.is_absolute()) {
        calib = fs::current_path() / calib;
      }
      std::array<char, MAX_PATHNAME_LENGTH> dir{};
      std::snprintf(dir.data(), dir.size(), "%s", calib.string().c_str());
      const auto err =
          SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dir.data());
      if (err != SAMPIC256CH_Success) {
        std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                  << ")\n";
      }
    }
  }

  void allocate_event_memory() {
    check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_),
          "AllocateEventMemory");
  }

  void configure_defaults() {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
          "DisableAllChannels");

    const auto channel_trigger_mode =
        use_self_trigger_channels_
            ? SAMPIC_CHANNEL_SELF_TRIGGER_MODE
            : SAMPIC_CHANNEL_EXT_TRIGGER_MODE;
    check(SAMPIC256CH_SetSampicChannelTriggerMode(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              channel_trigger_mode),
          "SetSampicChannelTriggerMode");

    if (use_self_trigger_channels_) {
      check(SAMPIC256CH_SetChannelSelflTriggerEdge(
                &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
                RISING_EDGE),
            "SetSelfTriggerEdge");
      check(SAMPIC256CH_SetSampicChannelPulseMode(
                &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
                TRUE),
            "SetPositivePulseMode");
    }

    check(SAMPIC256CH_SetSampicTriggerOption(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                             SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption");

    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_, trig_opts_.trigger_type),
          "SetExternalTriggerType");
    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, trig_opts_.trigger_edge),
          "SetExternalTriggerEdge");
    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, trig_opts_.trigger_level),
          "SetExternalTriggerSigLevel");
    check(SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, trig_opts_.sync_edge),
          "SetExternalSyncEdge");
    check(SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, trig_opts_.sync_level),
          "SetExternalSyncSigLevel");
    check(SAMPIC256CH_SetExternalTriggerCounterMode(&info_, &params_, TRUE, TRUE),
          "SetExternalTriggerCounterMode");
    check(SAMPIC256CH_SetSampicChannelInternalThreshold(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              static_cast<float>(conn_opts_.threshold_volts)),
          "SetSampicChannelInternalThreshold");
  }

  void check(SAMPIC256CH_ErrCode err, std::string_view what) {
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error(std::string(what) + " failed (code " +
                               std::to_string(static_cast<int>(err)) + ")");
    }
  }

  ConnectionConfig conn_opts_;
  ExternalTriggerConfig trig_opts_;
  CrateConnectionParamStruct conn_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool run_active_ = false;
  bool connected_ = false;
  bool use_self_trigger_channels_ = false;
  int sampling_frequency_requested_mhz_ = 0;
  int sampling_frequency_readback_mhz_ = 0;
  bool sampling_frequency_external_clock_ = false;
};
