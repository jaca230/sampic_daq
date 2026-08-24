#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <execinfo.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <deque>
#include <vector>

#include <unistd.h>

#include <nlohmann/json.hpp>

extern "C" {
#include <lpDevC.h>
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#if defined(__GNUC__)
#pragma weak LPD_ResetLostFrames
#pragma weak LPD_GetLostFrames
#pragma weak LPD_ResetTotalByteCount
#pragma weak LPD_GetTotalByteCount
#endif

#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"
#include "sampic_tests/lecroy/manual_trigger_controller.h"
#include "sampic_tests/batching_scan/batching_scan_config.h"
#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/probe_fatal_error.h"
#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_hit_associator.h"

namespace {

using sampic::double_pulse::ConnectionConfig;
using sampic::double_pulse::DoublePulseConfig;
using sampic::double_pulse::ExternalTriggerConfig;
using sampic::double_pulse::ParameterCombination;
using sampic::double_pulse::ReadoutConfig;
using sampic::double_pulse::StartRetryConfig;

volatile std::sig_atomic_t g_stop_after_current_point = 0;

void batching_scan_signal_handler(int) {
  g_stop_after_current_point = 1;
}

void crash_signal_handler(int signal_number) {
  static constexpr char message[] =
      "\nFatal signal in external_trigger_probe; native backtrace:\n";
  const auto bytes_written =
      ::write(STDERR_FILENO, message, sizeof(message) - 1);
  (void)bytes_written;
  void* frames[64];
  const int frame_count = ::backtrace(frames, 64);
  ::backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);
  ::_exit(128 + signal_number);
}

void install_crash_signal_handlers() {
  std::signal(SIGSEGV, crash_signal_handler);
  std::signal(SIGABRT, crash_signal_handler);
  std::signal(SIGBUS, crash_signal_handler);
  std::signal(SIGILL, crash_signal_handler);
}

struct ReadEventTiming {
  std::uint64_t read_buffer_calls = 0;
  double prepare_event_us = 0.0;
  double read_buffer_us = 0.0;
  double decode_event_us = 0.0;
  double requested_retry_sleep_us = 0.0;
};

struct RawVendorEvent {
  std::vector<ML_Frame> frames;
  std::vector<unsigned char> payload;
  int bytes = 0;
  int event_index = 0;
  bool during_drain = false;
  ReadEventTiming vendor_timing;
  double read_call_us = 0.0;
  double successful_read_gap_us = 0.0;

  void copy_from(const ML_Frame* source, int frame_count) {
    frames.assign(source, source + frame_count);
    std::size_t payload_bytes = 0;
    for (int index = 0; index < frame_count; ++index) {
      if (source[index].data_size > 0) {
        payload_bytes += static_cast<std::size_t>(source[index].data_size);
      }
    }
    payload.resize(payload_bytes);
    std::size_t offset = 0;
    for (int index = 0; index < frame_count; ++index) {
      const int size = source[index].data_size;
      if (size <= 0) {
        frames[index].user_data = nullptr;
        continue;
      }
      if (!source[index].user_data) {
        throw std::runtime_error("Vendor frame has a null payload pointer");
      }
      std::memcpy(payload.data() + offset, source[index].user_data,
                  static_cast<std::size_t>(size));
      frames[index].user_data = payload.data() + offset;
      offset += static_cast<std::size_t>(size);
    }
    bytes = static_cast<int>(payload_bytes);
  }
};

class RawVendorEventQueue {
 public:
  explicit RawVendorEventQueue(std::size_t capacity) {
    storage_.reserve(capacity);
    for (std::size_t index = 0; index < capacity; ++index) {
      auto event = std::make_unique<RawVendorEvent>();
      event->frames.reserve(MAX_EXPECTED_FRAMES);
      event->payload.reserve(65536);
      free_.push_back(event.get());
      storage_.push_back(std::move(event));
    }
  }

  RawVendorEvent* acquire() {
    std::unique_lock lock(mutex_);
    const bool blocked = free_.empty() && !closed_;
    const auto wait_start = std::chrono::steady_clock::now();
    free_available_.wait(lock, [&] { return closed_ || !free_.empty(); });
    if (blocked) {
      producer_wait_us_ += std::chrono::duration<double, std::micro>(
                               std::chrono::steady_clock::now() - wait_start)
                               .count();
    }
    if (closed_) return nullptr;
    auto* event = free_.front();
    free_.pop_front();
    return event;
  }

  void publish(RawVendorEvent* event) {
    {
      std::lock_guard lock(mutex_);
      ready_.push_back(event);
      high_water_mark_ = std::max(high_water_mark_, ready_.size());
    }
    ready_available_.notify_one();
  }

  RawVendorEvent* pop() {
    std::unique_lock lock(mutex_);
    ready_available_.wait(lock, [&] { return closed_ || !ready_.empty(); });
    if (ready_.empty()) return nullptr;
    auto* event = ready_.front();
    ready_.pop_front();
    return event;
  }

  void release(RawVendorEvent* event) {
    {
      std::lock_guard lock(mutex_);
      free_.push_back(event);
    }
    free_available_.notify_one();
  }

  void close() {
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
    }
    free_available_.notify_all();
    ready_available_.notify_all();
  }

  std::size_t high_water_mark() const {
    std::lock_guard lock(mutex_);
    return high_water_mark_;
  }

  double producer_wait_us() const {
    std::lock_guard lock(mutex_);
    return producer_wait_us_;
  }

 private:
  std::vector<std::unique_ptr<RawVendorEvent>> storage_;
  std::deque<RawVendorEvent*> free_;
  std::deque<RawVendorEvent*> ready_;
  mutable std::mutex mutex_;
  std::condition_variable free_available_;
  std::condition_variable ready_available_;
  bool closed_ = false;
  std::size_t high_water_mark_ = 0;
  double producer_wait_us_ = 0.0;
};

struct CollectionTimingRecord {
  int event_index = 0;
  bool during_drain = false;
  int hits = 0;
  int frames = 0;
  int bytes = 0;
  ReadEventTiming vendor;
  double read_call_us = 0.0;
  double processing_us = 0.0;
  double successful_read_gap_us = 0.0;
};

struct Options {
  std::string config_path;
  std::string output_dir;
  std::string lecroy_output_channel;
  int max_events = 50;
  double max_duration_s = 10.0;
  double excitation_wall_s = 0.0;
  bool use_self_trigger_channels = false;
  bool use_l2_external_gate = false;
  bool skip_lecroy = false;
  bool manage_lecroy_output = false;
  std::optional<double> lecroy_rate_hz;
  std::optional<double> lecroy_rate_readback_hz;
  bool summary_only = false;
  bool all_channels = false;
  bool persistent_session = false;
  bool pipelined_decode = false;
  std::size_t raw_queue_capacity = 128;
  std::size_t raw_queue_high_water_mark = 0;
  double raw_queue_producer_wait_us = 0.0;
  double drain_quiet_ms = 100.0;
  double drain_timeout_s = 10.0;
  int primitive_gate_clocks = 10;
  int latency_gate_clocks = 3;
  int external_gate_clocks = 5;
  int frames_per_block = 1;
  int triggers_per_event = 1;
  double hit_time_offset_ns = -470.0;
  double requested_hit_time_offset_ns = -470.0;
  bool auto_hit_offset = false;
  double pre_window_ns = 20.0;
  double post_window_ns = 20.0;
  std::vector<CollectionTimingRecord> collection_timings;
};

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + std::string(name));
      }
      return std::string(argv[++i]);
    };
    if (arg == "--config") {
      opts.config_path = require_value(arg);
    } else if (arg == "--output-dir") {
      opts.output_dir = require_value(arg);
    } else if (arg == "--events") {
      opts.max_events = std::stoi(require_value(arg));
    } else if (arg == "--duration") {
      opts.max_duration_s = std::stod(require_value(arg));
    } else if (arg == "--self-trigger-channels") {
      opts.use_self_trigger_channels = true;
    } else if (arg == "--l2-external-gate") {
      opts.use_l2_external_gate = true;
    } else if (arg == "--skip-lecroy") {
      opts.skip_lecroy = true;
    } else if (arg == "--manage-lecroy-output") {
      opts.manage_lecroy_output = true;
    } else if (arg == "--lecroy-output-channel") {
      opts.lecroy_output_channel = require_value(arg);
    } else if (arg == "--lecroy-rate-hz") {
      opts.lecroy_rate_hz = std::stod(require_value(arg));
    } else if (arg == "--drain-quiet-ms") {
      opts.drain_quiet_ms = std::stod(require_value(arg));
    } else if (arg == "--drain-timeout-s") {
      opts.drain_timeout_s = std::stod(require_value(arg));
    } else if (arg == "--summary-only") {
      opts.summary_only = true;
    } else if (arg == "--all-channels") {
      opts.all_channels = true;
    } else if (arg == "--primitive-gate-clocks") {
      opts.primitive_gate_clocks = std::stoi(require_value(arg));
    } else if (arg == "--latency-gate-clocks") {
      opts.latency_gate_clocks = std::stoi(require_value(arg));
    } else if (arg == "--external-gate-clocks") {
      opts.external_gate_clocks = std::stoi(require_value(arg));
    } else if (arg == "--frames-per-block") {
      opts.frames_per_block = std::stoi(require_value(arg));
    } else if (arg == "--triggers-per-event") {
      opts.triggers_per_event = std::stoi(require_value(arg));
    } else if (arg == "--hit-offset-ns") {
      opts.hit_time_offset_ns = std::stod(require_value(arg));
      opts.requested_hit_time_offset_ns = opts.hit_time_offset_ns;
    } else if (arg == "--auto-hit-offset") {
      opts.auto_hit_offset = true;
    } else if (arg == "--pre-window-ns") {
      opts.pre_window_ns = std::stod(require_value(arg));
    } else if (arg == "--post-window-ns") {
      opts.post_window_ns = std::stod(require_value(arg));
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: external_trigger_probe --config <file> [--events N] "
                   "[--duration seconds] [--self-trigger-channels] [--skip-lecroy] "
                   "[--manage-lecroy-output] [--lecroy-output-channel name] "
                   "[--lecroy-rate-hz N] "
                   "[--drain-quiet-ms N] "
                   "[--drain-timeout-s N] "
                   "[--output-dir path] "
                   "[--l2-external-gate] [--primitive-gate-clocks N] "
                   "[--latency-gate-clocks N] [--external-gate-clocks N] "
                   "[--frames-per-block N] [--triggers-per-event N] "
                   "[--hit-offset-ns N] [--auto-hit-offset] [--pre-window-ns N] "
                   "[--post-window-ns N] [--all-channels] [--summary-only]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  if (opts.config_path.empty()) {
    throw std::runtime_error("external_trigger_probe requires --config <file>");
  }
  if (opts.max_events <= 0) {
    throw std::runtime_error("--events must be positive");
  }
  if (opts.drain_quiet_ms <= 0.0 || opts.drain_timeout_s <= 0.0) {
    throw std::runtime_error(
        "--drain-quiet-ms and --drain-timeout-s must be positive");
  }
  if (opts.lecroy_rate_hz && *opts.lecroy_rate_hz <= 0.0) {
    throw std::runtime_error("--lecroy-rate-hz must be positive");
  }
  if (opts.use_l2_external_gate && !opts.use_self_trigger_channels) {
    throw std::runtime_error(
        "--l2-external-gate requires --self-trigger-channels");
  }
  if (opts.primitive_gate_clocks < 0 || opts.primitive_gate_clocks > 255 ||
      opts.latency_gate_clocks < 0 || opts.latency_gate_clocks > 255 ||
      opts.external_gate_clocks < 3 || opts.external_gate_clocks > 255) {
    throw std::runtime_error(
        "L2 gate clocks must fit in one byte and --external-gate-clocks "
        "must be at least 3");
  }
  if (opts.frames_per_block < 1 || opts.frames_per_block > 31 ||
      opts.triggers_per_event < 1 || opts.triggers_per_event > 127) {
    throw std::runtime_error(
        "--frames-per-block must be in [1, 31] and "
        "--triggers-per-event must be in [1, 127]");
  }
  return opts;
}

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
};

const char* disposition_name(ExternalTriggerHitDisposition disposition) {
  switch (disposition) {
    case ExternalTriggerHitDisposition::Assigned:
      return "assigned";
    case ExternalTriggerHitDisposition::NoTriggerRecords:
      return "dropped:no-trigger-records";
    case ExternalTriggerHitDisposition::OutsideAssociationWindow:
      return "dropped:outside-window";
  }
  return "unknown";
}

void add_stats(
    ExternalTriggerAssociationStats& total,
    const ExternalTriggerAssociationStats& event) {
  total.raw_hits += event.raw_hits;
  total.trigger_records += event.trigger_records;
  total.assigned_hits += event.assigned_hits;
  total.hits_without_trigger_records += event.hits_without_trigger_records;
  total.hits_outside_window += event.hits_outside_window;
  total.hits_inside_multiple_windows += event.hits_inside_multiple_windows;
  total.triggers_with_hits += event.triggers_with_hits;
  total.triggers_without_hits += event.triggers_without_hits;
}

void print_event(
    int index,
    const EventStruct& event,
    int decoded_hits,
    int frames,
    int bytes,
    const ExternalTriggerAssociationResult& association) {
  std::cout << "Event " << index
            << ": decoded_hits=" << decoded_hits
            << " event_hits=" << event.NbOfHitsInEvent
            << " triggers=" << event.TriggerData.NbOfTriggers
            << " assigned=" << association.stats.assigned_hits
            << " dropped_no_trigger=" << association.stats.hits_without_trigger_records
            << " dropped_outside_window=" << association.stats.hits_outside_window
            << " ambiguous=" << association.stats.hits_inside_multiple_windows
            << " frames=" << frames
            << " bytes=" << bytes << "\n";

  for (int i = 0; i < event.TriggerData.NbOfTriggers; ++i) {
    std::cout << "  trigger[" << i << "]: fpga_id="
              << event.TriggerData.TriggerIDFromFPGA[i]
              << " external_id=" << event.TriggerData.TriggerIDFromExtTrig[i]
              << " timestamp_ns=" << std::scientific
              << event.TriggerData.TriggerTimeStamp[i] << std::defaultfloat
              << " assigned_hits=" << association.hits_by_trigger[i].size()
              << " ambiguous_hits=" << association.ambiguous_hits_by_trigger[i]
              << "\n";
  }

  const int capped_hits = std::min(event.NbOfHitsInEvent, MAX_EXPECTED_FRAMES);
  for (int i = 0; i < capped_hits; ++i) {
    const auto& hit = event.Hit[i];
    const auto& decision = association.hit_decisions[static_cast<std::size_t>(i)];
    std::cout << "    hit[" << i << "]: FEB=" << hit.FeBoardIndex
              << " sampic=" << hit.SampicIndex
              << " channel=" << hit.Channel
              << " first_cell_ts(ns)=" << std::scientific << hit.FirstCellTimeStamp
              << " association_ts(ns)=" << decision.hit_timestamp_ns
              << " amplitude=" << hit.Amplitude
              << " tot(ns)=" << hit.TOTValue << std::defaultfloat
              << " result=" << disposition_name(decision.disposition);
    if (decision.nearest_trigger_index) {
      std::cout << " nearest_trigger=" << *decision.nearest_trigger_index
                << " hit_minus_reference_ns=" << std::fixed
                << std::setprecision(3) << decision.hit_minus_reference_ns
                << std::defaultfloat
                << " windows_matched=" << decision.triggers_inside_window;
    }
    std::cout << "\n";
  }
}

void print_summary(
    int decoded_events,
    const ExternalTriggerAssociationStats& stats) {
  const double assignment_percent =
      stats.raw_hits == 0
          ? 0.0
          : 100.0 * static_cast<double>(stats.assigned_hits) /
                static_cast<double>(stats.raw_hits);

  std::cout << "\n=== External-trigger association summary ===\n"
            << "Decoded vendor events:           " << decoded_events << "\n"
            << "Raw vendor hits:                 " << stats.raw_hits << "\n"
            << "Raw external-trigger records:    " << stats.trigger_records << "\n"
            << "Assigned hits:                   " << stats.assigned_hits
            << " (" << std::fixed << std::setprecision(2)
            << assignment_percent << "%)\n"
            << "Dropped: no trigger records:     "
            << stats.hits_without_trigger_records << "\n"
            << "Dropped: outside time window:    "
            << stats.hits_outside_window << "\n"
            << "Hits inside multiple windows:    "
            << stats.hits_inside_multiple_windows << "\n"
            << "Trigger records with hits:       "
            << stats.triggers_with_hits << "\n"
            << "Trigger records without hits:    "
            << stats.triggers_without_hits << "\n"
            << std::defaultfloat;
}

struct ObservedHit {
  int event_index = 0;
  int hit_index = 0;
  int feb = 0;
  int sampic = 0;
  int channel = 0;
  double timestamp_ns = 0.0;
  double first_cell_timestamp_ns = 0.0;
  int trigger_position_cell = 0;
};

struct ObservedTrigger {
  int event_index = 0;
  int trigger_index = 0;
  int fpga_id = 0;
  unsigned int external_id = 0;
  double timestamp_ns = 0.0;
};

ExternalTriggerAssociationStats summarize_same_vendor_event_association(
    const Options& options,
    const std::vector<ObservedHit>& hits,
    const std::vector<ObservedTrigger>& triggers) {
  ExternalTriggerAssociationStats stats;
  stats.raw_hits = hits.size();
  stats.trigger_records = triggers.size();

  std::size_t hit_begin = 0;
  std::size_t trigger_begin = 0;
  while (hit_begin < hits.size() || trigger_begin < triggers.size()) {
    int event_index = std::numeric_limits<int>::max();
    if (hit_begin < hits.size()) {
      event_index = std::min(event_index, hits[hit_begin].event_index);
    }
    if (trigger_begin < triggers.size()) {
      event_index = std::min(event_index, triggers[trigger_begin].event_index);
    }

    std::size_t hit_end = hit_begin;
    while (hit_end < hits.size() &&
           hits[hit_end].event_index == event_index) {
      ++hit_end;
    }
    std::size_t trigger_end = trigger_begin;
    while (trigger_end < triggers.size() &&
           triggers[trigger_end].event_index == event_index) {
      ++trigger_end;
    }
    std::vector<bool> trigger_has_hits(trigger_end - trigger_begin, false);

    for (std::size_t hit_index = hit_begin; hit_index < hit_end; ++hit_index) {
      if (trigger_begin == trigger_end) {
        ++stats.hits_without_trigger_records;
        continue;
      }
      std::size_t nearest_trigger = trigger_begin;
      double nearest_distance = std::numeric_limits<double>::infinity();
      std::size_t triggers_inside_window = 0;
      for (std::size_t trigger_index = trigger_begin;
           trigger_index < trigger_end;
           ++trigger_index) {
        const double reference =
            triggers[trigger_index].timestamp_ns + options.hit_time_offset_ns;
        const double delta = hits[hit_index].timestamp_ns - reference;
        const double distance = std::abs(delta);
        if (distance < nearest_distance) {
          nearest_distance = distance;
          nearest_trigger = trigger_index;
        }
        if (delta >= -options.pre_window_ns &&
            delta <= options.post_window_ns) {
          ++triggers_inside_window;
        }
      }
      const double nearest_reference =
          triggers[nearest_trigger].timestamp_ns + options.hit_time_offset_ns;
      const double residual =
          hits[hit_index].timestamp_ns - nearest_reference;
      if (residual < -options.pre_window_ns ||
          residual > options.post_window_ns) {
        ++stats.hits_outside_window;
        continue;
      }
      ++stats.assigned_hits;
      trigger_has_hits[nearest_trigger - trigger_begin] = true;
      if (triggers_inside_window > 1) {
        ++stats.hits_inside_multiple_windows;
      }
    }
    stats.triggers_with_hits += static_cast<std::size_t>(std::count(
        trigger_has_hits.begin(), trigger_has_hits.end(), true));

    hit_begin = hit_end;
    trigger_begin = trigger_end;
  }
  stats.triggers_without_hits =
      stats.trigger_records - stats.triggers_with_hits;
  return stats;
}

struct HitMatchRecord {
  std::size_t nearest_trigger = 0;
  double target_trigger_timestamp_ns = 0.0;
  double hit_minus_trigger_ns = 0.0;
  double residual_ns = 0.0;
  bool accepted = false;
  std::string coverage;
  int packet_lag = 0;
};

std::optional<double> estimate_hit_time_offset_ns(
    const std::vector<ObservedHit>& hits,
    const std::vector<ObservedTrigger>& triggers) {
  if (hits.empty() || triggers.empty()) {
    return std::nullopt;
  }

  std::vector<double> trigger_timestamps;
  trigger_timestamps.reserve(triggers.size());
  for (const auto& trigger : triggers) {
    trigger_timestamps.push_back(trigger.timestamp_ns);
  }
  std::sort(trigger_timestamps.begin(), trigger_timestamps.end());

  // The physical hit/trigger latency is sub-microsecond in these setups. A
  // generous 10 us guard excludes capture-tail hits whose corresponding
  // trigger record is absent, so those cannot drag the estimate away from the
  // narrow correlation peak.
  constexpr double maximum_candidate_offset_ns = 10000.0;
  std::vector<double> candidates;
  candidates.reserve(hits.size());
  for (const auto& hit : hits) {
    const auto upper = std::lower_bound(
        trigger_timestamps.begin(), trigger_timestamps.end(), hit.timestamp_ns);
    auto best = upper;
    if (upper == trigger_timestamps.end() ||
        (upper != trigger_timestamps.begin() &&
         std::abs(*(upper - 1) - hit.timestamp_ns) <
             std::abs(*upper - hit.timestamp_ns))) {
      best = upper - 1;
    }
    const double difference_ns = hit.timestamp_ns - *best;
    if (std::abs(difference_ns) <= maximum_candidate_offset_ns) {
      candidates.push_back(difference_ns);
    }
  }
  if (candidates.empty()) {
    return std::nullopt;
  }

  const auto middle = candidates.begin() + candidates.size() / 2;
  std::nth_element(candidates.begin(), middle, candidates.end());
  double median = *middle;
  if (candidates.size() % 2 == 0) {
    const auto lower = std::max_element(candidates.begin(), middle);
    median = (*lower + *middle) / 2.0;
  }
  return median;
}

void write_diagnostic_export(
    const Options& options,
    int decoded_events,
    int events_before_cutoff,
    bool vendor_transport_counters_available,
    unsigned long lost_transport_frames,
    unsigned long transport_bytes,
    std::uint64_t decoded_readout_bytes,
    std::uint64_t decoded_readout_frames,
    double capture_wall_s,
    const std::vector<ObservedHit>& hits,
    const std::vector<ObservedTrigger>& triggers,
    const std::vector<HitMatchRecord>& hit_matches,
    const std::vector<std::size_t>& hits_per_trigger,
    const std::vector<int>& first_hit_event_by_trigger,
    const std::vector<int>& last_hit_event_by_trigger) {
  if (options.output_dir.empty()) {
    return;
  }

  const std::filesystem::path output_dir{options.output_dir};
  std::filesystem::create_directories(output_dir);

  auto open_output = [&](std::string_view filename) {
    std::ofstream stream(output_dir / filename);
    if (!stream) {
      throw std::runtime_error(
          "Unable to open diagnostic export file: " +
          (output_dir / filename).string());
    }
    stream << std::setprecision(17);
    return stream;
  };

  {
    auto stream = open_output("hits.csv");
    stream
        << "event_index,hit_index,feb,sampic,channel,timestamp_ns,"
           "first_cell_timestamp_ns,trigger_position_cell,"
           "target_trigger_timestamp_ns,nearest_trigger_sorted_index,"
           "nearest_trigger_event_index,nearest_trigger_index,nearest_fpga_id,"
           "nearest_external_id,nearest_trigger_timestamp_ns,"
           "hit_minus_trigger_ns,residual_ns,accepted,coverage,packet_lag\n";
    for (std::size_t index = 0; index < hits.size(); ++index) {
      const auto& hit = hits[index];
      const auto& match = hit_matches[index];
      const auto& trigger = triggers[match.nearest_trigger];
      stream << hit.event_index << "," << hit.hit_index << "," << hit.feb
             << "," << hit.sampic << "," << hit.channel << ","
             << hit.timestamp_ns << "," << hit.first_cell_timestamp_ns << ","
             << hit.trigger_position_cell << ","
             << match.target_trigger_timestamp_ns << ","
             << match.nearest_trigger << "," << trigger.event_index << ","
             << trigger.trigger_index << "," << trigger.fpga_id << ","
             << trigger.external_id << "," << trigger.timestamp_ns << ","
             << match.hit_minus_trigger_ns << "," << match.residual_ns << ","
             << (match.accepted ? 1 : 0) << "," << match.coverage << ","
             << match.packet_lag << "\n";
    }
  }

  {
    auto stream = open_output("triggers.csv");
    stream
        << "sorted_trigger_index,event_index,trigger_index,fpga_id,external_id,"
           "timestamp_ns,assigned_hits,first_hit_event,last_hit_event\n";
    for (std::size_t index = 0; index < triggers.size(); ++index) {
      const auto& trigger = triggers[index];
      stream << index << "," << trigger.event_index << ","
             << trigger.trigger_index << "," << trigger.fpga_id << ","
             << trigger.external_id << "," << trigger.timestamp_ns << ","
             << hits_per_trigger[index] << ",";
      if (hits_per_trigger[index] > 0) {
        stream << first_hit_event_by_trigger[index] << ","
               << last_hit_event_by_trigger[index];
      } else {
        stream << ",";
      }
      stream << "\n";
    }
  }

  struct PacketExport {
    std::size_t hit_count = 0;
    std::size_t trigger_count = 0;
    double hit_min_ns = std::numeric_limits<double>::infinity();
    double hit_max_ns = -std::numeric_limits<double>::infinity();
    double hit_sum_ns = 0.0;
    double trigger_min_ns = std::numeric_limits<double>::infinity();
    double trigger_max_ns = -std::numeric_limits<double>::infinity();
    double trigger_sum_ns = 0.0;
  };
  std::map<int, PacketExport> packets;
  for (const auto& hit : hits) {
    auto& packet = packets[hit.event_index];
    ++packet.hit_count;
    packet.hit_min_ns = std::min(packet.hit_min_ns, hit.timestamp_ns);
    packet.hit_max_ns = std::max(packet.hit_max_ns, hit.timestamp_ns);
    packet.hit_sum_ns += hit.timestamp_ns;
  }
  for (const auto& trigger : triggers) {
    auto& packet = packets[trigger.event_index];
    ++packet.trigger_count;
    packet.trigger_min_ns =
        std::min(packet.trigger_min_ns, trigger.timestamp_ns);
    packet.trigger_max_ns =
        std::max(packet.trigger_max_ns, trigger.timestamp_ns);
    packet.trigger_sum_ns += trigger.timestamp_ns;
  }
  {
    auto stream = open_output("packets.csv");
    stream
        << "event_index,hit_count,trigger_count,hit_timestamp_min_ns,"
           "hit_timestamp_mean_ns,hit_timestamp_max_ns,"
           "trigger_timestamp_min_ns,trigger_timestamp_mean_ns,"
           "trigger_timestamp_max_ns\n";
    for (const auto& [event_index, packet] : packets) {
      stream << event_index << "," << packet.hit_count << ","
             << packet.trigger_count << ",";
      if (packet.hit_count > 0) {
        stream << packet.hit_min_ns << ","
               << packet.hit_sum_ns / static_cast<double>(packet.hit_count)
               << "," << packet.hit_max_ns;
      } else {
        stream << ",,";
      }
      stream << ",";
      if (packet.trigger_count > 0) {
        stream
            << packet.trigger_min_ns << ","
            << packet.trigger_sum_ns /
                   static_cast<double>(packet.trigger_count)
            << "," << packet.trigger_max_ns;
      } else {
        stream << ",,";
      }
      stream << "\n";
    }
  }

  {
    auto stream = open_output("collection_timing.csv");
    stream
        << "event_index,phase,hits,frames,bytes,read_call_us,"
           "prepare_event_us,read_buffer_us,decode_event_us,"
           "requested_retry_sleep_us,read_buffer_calls,processing_us,"
           "successful_read_gap_us\n";
    for (const auto& timing : options.collection_timings) {
      stream << timing.event_index << ","
             << (timing.during_drain ? "drain" : "active") << ","
             << timing.hits << "," << timing.frames << "," << timing.bytes
             << "," << timing.read_call_us << ","
             << timing.vendor.prepare_event_us << ","
             << timing.vendor.read_buffer_us << ","
             << timing.vendor.decode_event_us << ","
             << timing.vendor.requested_retry_sleep_us << ","
             << timing.vendor.read_buffer_calls << ","
             << timing.processing_us << ","
             << timing.successful_read_gap_us << "\n";
    }
  }

  std::vector<const CollectionTimingRecord*> active_timings;
  active_timings.reserve(options.collection_timings.size());
  for (const auto& timing : options.collection_timings) {
    if (!timing.during_drain) active_timings.push_back(&timing);
  }
  auto timing_values = [&](auto member) {
    std::vector<double> values;
    values.reserve(active_timings.size());
    for (const auto* timing : active_timings) values.push_back(member(*timing));
    return values;
  };
  auto sum = [](const std::vector<double>& values) {
    double result = 0.0;
    for (const double value : values) result += value;
    return result;
  };
  auto percentile = [](std::vector<double> values, double fraction) {
    if (values.empty()) return 0.0;
    const auto index = static_cast<std::size_t>(std::floor(
        fraction * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
  };
  const auto read_call_us = timing_values(
      [](const auto& timing) { return timing.read_call_us; });
  const auto processing_us = timing_values(
      [](const auto& timing) { return timing.processing_us; });
  const auto read_gap_us = timing_values(
      [](const auto& timing) { return timing.successful_read_gap_us; });
  const double read_call_total_us = sum(read_call_us);
  const double processing_total_us = sum(processing_us);
  std::uint64_t read_buffer_calls = 0;
  double prepare_event_total_us = 0.0;
  double read_buffer_total_us = 0.0;
  double decode_event_total_us = 0.0;
  double requested_retry_sleep_total_us = 0.0;
  for (const auto* timing : active_timings) {
    read_buffer_calls += timing->vendor.read_buffer_calls;
    prepare_event_total_us += timing->vendor.prepare_event_us;
    read_buffer_total_us += timing->vendor.read_buffer_us;
    decode_event_total_us += timing->vendor.decode_event_us;
    requested_retry_sleep_total_us +=
        timing->vendor.requested_retry_sleep_us;
  }
  const double active_wall_us = options.excitation_wall_s * 1.0e6;
  const auto timing_summary = nlohmann::json{
      {"acquisition_model",
       options.pipelined_decode ? "pipelined_receive_decode" : "synchronous"},
      {"raw_queue_capacity", options.raw_queue_capacity},
      {"raw_queue_high_water_mark", options.raw_queue_high_water_mark},
      {"raw_queue_producer_wait_s",
       options.raw_queue_producer_wait_us / 1.0e6},
      {"active_events", active_timings.size()},
      {"drain_events",
       options.collection_timings.size() - active_timings.size()},
      {"active_wall_s", options.excitation_wall_s},
      {"read_call_total_s", read_call_total_us / 1.0e6},
      {"event_processing_total_s", processing_total_us / 1.0e6},
      {"read_call_fraction_of_active_wall",
       active_wall_us > 0.0 ? read_call_total_us / active_wall_us : 0.0},
      {"event_processing_fraction_of_active_wall",
       active_wall_us > 0.0 ? processing_total_us / active_wall_us : 0.0},
      {"read_buffer_calls", read_buffer_calls},
      {"read_buffer_calls_per_active_event",
       active_timings.empty()
           ? 0.0
           : static_cast<double>(read_buffer_calls) /
                 static_cast<double>(active_timings.size())},
      {"prepare_event_total_s", prepare_event_total_us / 1.0e6},
      {"read_buffer_total_s", read_buffer_total_us / 1.0e6},
      {"decode_event_total_s", decode_event_total_us / 1.0e6},
      {"requested_retry_sleep_total_s",
       requested_retry_sleep_total_us / 1.0e6},
      {"read_call_us",
       {{"mean", read_call_us.empty()
                    ? 0.0
                    : read_call_total_us / read_call_us.size()},
        {"p95", percentile(read_call_us, 0.95)},
        {"p99", percentile(read_call_us, 0.99)},
        {"max", read_call_us.empty()
                    ? 0.0
                    : *std::max_element(
                          read_call_us.begin(), read_call_us.end())}}},
      {"event_processing_us",
       {{"mean", processing_us.empty()
                    ? 0.0
                    : processing_total_us / processing_us.size()},
        {"p95", percentile(processing_us, 0.95)},
        {"p99", percentile(processing_us, 0.99)},
        {"max", processing_us.empty()
                    ? 0.0
                    : *std::max_element(
                          processing_us.begin(), processing_us.end())}}},
      {"successful_read_gap_us",
       {{"p50", percentile(read_gap_us, 0.50)},
        {"p95", percentile(read_gap_us, 0.95)},
        {"p99", percentile(read_gap_us, 0.99)},
        {"max", read_gap_us.empty()
                    ? 0.0
                    : *std::max_element(
                          read_gap_us.begin(), read_gap_us.end())}}}};

  const auto generated_at_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  nlohmann::json metadata{
      {"schema_version", 2},
      {"generated_at_unix_ms", generated_at_ms},
      {"config_path", options.config_path},
      {"output_dir", std::filesystem::absolute(output_dir).string()},
      {"acquisition",
       {
           {"decoded_vendor_events", decoded_events},
           {"events_before_cutoff", events_before_cutoff},
           {"drained_vendor_events", decoded_events - events_before_cutoff},
           {"requested_events", options.max_events},
           {"requested_duration_s", options.max_duration_s},
           {"excitation_wall_s", options.excitation_wall_s},
           {"self_trigger_channels", options.use_self_trigger_channels},
           {"l2_external_gate", options.use_l2_external_gate},
           {"manage_lecroy_output", options.manage_lecroy_output},
           {"lecroy_output_channel", options.lecroy_output_channel},
           {"lecroy_requested_rate_hz",
            options.lecroy_rate_hz
                ? nlohmann::json(*options.lecroy_rate_hz)
                : nlohmann::json(nullptr)},
           {"lecroy_readback_rate_hz",
            options.lecroy_rate_readback_hz
                ? nlohmann::json(*options.lecroy_rate_readback_hz)
                : nlohmann::json(nullptr)},
           {"drain_quiet_ms", options.drain_quiet_ms},
           {"drain_timeout_s", options.drain_timeout_s},
           {"all_channels", options.all_channels},
           {"persistent_session", options.persistent_session},
           {"primitive_gate_clocks", options.primitive_gate_clocks},
           {"latency_gate_clocks", options.latency_gate_clocks},
           {"external_gate_clocks", options.external_gate_clocks},
           {"frames_per_block", options.frames_per_block},
           {"triggers_per_event", options.triggers_per_event},
       }},
      {"transport",
       {
           {"vendor_counters_available",
            vendor_transport_counters_available},
           {"vendor_lost_frames", lost_transport_frames},
           {"vendor_total_bytes", transport_bytes},
           {"decoded_readout_bytes", decoded_readout_bytes},
           {"decoded_readout_frames", decoded_readout_frames},
           {"capture_wall_s", capture_wall_s},
           {"vendor_bytes_per_s",
            capture_wall_s > 0.0
                ? static_cast<double>(transport_bytes) / capture_wall_s
                : 0.0},
       }},
      {"collection_timing", timing_summary},
      {"association",
       {
           {"requested_hit_time_offset_ns",
            options.requested_hit_time_offset_ns},
           {"hit_time_offset_ns", options.hit_time_offset_ns},
           {"auto_hit_offset", options.auto_hit_offset},
           {"pre_window_ns", options.pre_window_ns},
           {"post_window_ns", options.post_window_ns},
       }},
      {"counts",
       {
           {"hits", hits.size()},
           {"triggers", triggers.size()},
           {"accepted_hits",
            std::count_if(
                hit_matches.begin(),
                hit_matches.end(),
                [](const auto& match) { return match.accepted; })},
           {"populated_triggers",
            std::count_if(
                hits_per_trigger.begin(),
                hits_per_trigger.end(),
                [](std::size_t count) { return count > 0; })},
       }},
      {"files",
       {
           {"hits", "hits.csv"},
           {"triggers", "triggers.csv"},
           {"packets", "packets.csv"},
           {"collection_timing", "collection_timing.csv"},
       }}};
  {
    auto stream = open_output("metadata.json");
    stream << metadata.dump(2) << "\n";
  }

  std::cout << "Diagnostic data exported to "
            << std::filesystem::absolute(output_dir) << "\n";
}

void print_stream_correlation(
    const Options& options,
    int decoded_events,
    int events_before_cutoff,
    bool vendor_transport_counters_available,
    unsigned long lost_transport_frames,
    unsigned long transport_bytes,
    std::uint64_t decoded_readout_bytes,
    std::uint64_t decoded_readout_frames,
    double capture_wall_s,
    std::vector<ObservedHit> hits,
    std::vector<ObservedTrigger> triggers) {
  std::cout << "\n=== Stream-wide nearest-trigger diagnostic ===\n";
  if (hits.empty()) {
    std::cout << "No vendor hits were decoded.\n";
    const std::vector<HitMatchRecord> hit_matches;
    const std::vector<std::size_t> hits_per_trigger(triggers.size(), 0);
    const std::vector<int> first_hit_event_by_trigger(
        triggers.size(), std::numeric_limits<int>::max());
    const std::vector<int> last_hit_event_by_trigger(
        triggers.size(), std::numeric_limits<int>::min());
    write_diagnostic_export(
        options,
        decoded_events,
        events_before_cutoff,
        vendor_transport_counters_available,
        lost_transport_frames,
        transport_bytes,
        decoded_readout_bytes,
        decoded_readout_frames,
        capture_wall_s,
        hits,
        triggers,
        hit_matches,
        hits_per_trigger,
        first_hit_event_by_trigger,
        last_hit_event_by_trigger);
    return;
  }
  if (triggers.empty()) {
    std::cout << "No external-trigger records were decoded; all " << hits.size()
              << " hits are uncorrelatable.\n";
    return;
  }

  struct PacketTimestampSummary {
    double minimum_ns = std::numeric_limits<double>::infinity();
    double maximum_ns = -std::numeric_limits<double>::infinity();
    double sum_ns = 0.0;
    std::size_t count = 0;
  };

  std::map<int, PacketTimestampSummary> hit_packet_timestamps;
  for (const auto& hit : hits) {
    auto& packet = hit_packet_timestamps[hit.event_index];
    packet.minimum_ns = std::min(packet.minimum_ns, hit.timestamp_ns);
    packet.maximum_ns = std::max(packet.maximum_ns, hit.timestamp_ns);
    packet.sum_ns += hit.timestamp_ns;
    ++packet.count;
  }

  std::size_t hit_packet_timestamp_inversions = 0;
  double largest_hit_packet_backward_jump_ns = 0.0;
  double previous_hit_packet_mean_ns = -std::numeric_limits<double>::infinity();
  double mean_hit_packet_span_ns = 0.0;
  double maximum_hit_packet_span_ns = 0.0;
  for (const auto& [event_index, packet] : hit_packet_timestamps) {
    (void)event_index;
    const double mean_ns = packet.sum_ns / static_cast<double>(packet.count);
    if (mean_ns < previous_hit_packet_mean_ns) {
      ++hit_packet_timestamp_inversions;
      largest_hit_packet_backward_jump_ns = std::max(
          largest_hit_packet_backward_jump_ns,
          previous_hit_packet_mean_ns - mean_ns);
    }
    previous_hit_packet_mean_ns = mean_ns;
    const double span_ns = packet.maximum_ns - packet.minimum_ns;
    mean_hit_packet_span_ns += span_ns;
    maximum_hit_packet_span_ns =
        std::max(maximum_hit_packet_span_ns, span_ns);
  }
  mean_hit_packet_span_ns /= static_cast<double>(hit_packet_timestamps.size());

  std::size_t trigger_timestamp_inversions = 0;
  double largest_trigger_backward_jump_ns = 0.0;
  double previous_trigger_timestamp_ns = -std::numeric_limits<double>::infinity();
  for (const auto& trigger : triggers) {
    if (trigger.timestamp_ns < previous_trigger_timestamp_ns) {
      ++trigger_timestamp_inversions;
      largest_trigger_backward_jump_ns = std::max(
          largest_trigger_backward_jump_ns,
          previous_trigger_timestamp_ns - trigger.timestamp_ns);
    }
    previous_trigger_timestamp_ns = trigger.timestamp_ns;
  }

  std::sort(
      triggers.begin(),
      triggers.end(),
      [](const auto& left, const auto& right) {
        return left.timestamp_ns < right.timestamp_ns;
      });

  std::size_t matched = 0;
  std::size_t matched_same_packet = 0;
  std::size_t matched_cross_packet = 0;
  std::size_t trigger_arrived_before_hit = 0;
  std::size_t hit_arrived_before_trigger = 0;
  std::size_t outside_window = 0;
  std::size_t unmatched_before_trigger_coverage = 0;
  std::size_t unmatched_after_trigger_coverage = 0;
  std::size_t unmatched_inside_trigger_coverage = 0;
  std::size_t targets_before_trigger_coverage = 0;
  std::size_t targets_after_trigger_coverage = 0;
  std::size_t targets_inside_trigger_coverage = 0;
  std::vector<std::size_t> hits_per_trigger(triggers.size(), 0);
  std::vector<int> first_hit_event_by_trigger(
      triggers.size(), std::numeric_limits<int>::max());
  std::vector<int> last_hit_event_by_trigger(
      triggers.size(), std::numeric_limits<int>::min());
  std::vector<HitMatchRecord> hit_matches;
  hit_matches.reserve(hits.size());
  std::map<int, std::size_t> vendor_event_lag_histogram;
  std::set<std::pair<int, int>> matched_packet_pairs;
  double nearest_residual_sum = 0.0;
  double nearest_residual_min = std::numeric_limits<double>::infinity();
  double nearest_residual_max = -std::numeric_limits<double>::infinity();
  double covered_residual_sum = 0.0;
  double covered_residual_min = std::numeric_limits<double>::infinity();
  double covered_residual_max = -std::numeric_limits<double>::infinity();
  double target_timestamp_min = std::numeric_limits<double>::infinity();
  double target_timestamp_max = -std::numeric_limits<double>::infinity();
  std::size_t examples_printed = 0;

  for (const auto& hit : hits) {
    const double target_trigger_ns =
        hit.timestamp_ns - options.hit_time_offset_ns;
    target_timestamp_min = std::min(target_timestamp_min, target_trigger_ns);
    target_timestamp_max = std::max(target_timestamp_max, target_trigger_ns);
    const bool target_before_trigger_coverage =
        target_trigger_ns < triggers.front().timestamp_ns;
    const bool target_after_trigger_coverage =
        target_trigger_ns > triggers.back().timestamp_ns;
    if (target_before_trigger_coverage) {
      ++targets_before_trigger_coverage;
    } else if (target_after_trigger_coverage) {
      ++targets_after_trigger_coverage;
    } else {
      ++targets_inside_trigger_coverage;
    }

    const auto upper = std::lower_bound(
        triggers.begin(),
        triggers.end(),
        target_trigger_ns,
        [](const ObservedTrigger& trigger, double target) {
          return trigger.timestamp_ns < target;
        });

    auto best = upper;
    if (upper == triggers.end() ||
        (upper != triggers.begin() &&
         std::abs((upper - 1)->timestamp_ns - target_trigger_ns) <
             std::abs(upper->timestamp_ns - target_trigger_ns))) {
      best = upper - 1;
    }

    const std::size_t trigger_position =
        static_cast<std::size_t>(std::distance(triggers.begin(), best));
    const double hit_minus_trigger_ns =
        hit.timestamp_ns - best->timestamp_ns;
    const double residual_ns =
        hit_minus_trigger_ns - options.hit_time_offset_ns;
    nearest_residual_sum += residual_ns;
    nearest_residual_min = std::min(nearest_residual_min, residual_ns);
    nearest_residual_max = std::max(nearest_residual_max, residual_ns);
    if (!target_before_trigger_coverage && !target_after_trigger_coverage) {
      covered_residual_sum += residual_ns;
      covered_residual_min = std::min(covered_residual_min, residual_ns);
      covered_residual_max = std::max(covered_residual_max, residual_ns);
    }

    const bool accepted =
        residual_ns >= -options.pre_window_ns &&
        residual_ns <= options.post_window_ns;
    if (accepted) {
      ++matched;
      ++hits_per_trigger[trigger_position];
      const int vendor_event_lag = hit.event_index - best->event_index;
      ++vendor_event_lag_histogram[vendor_event_lag];
      matched_packet_pairs.emplace(hit.event_index, best->event_index);
      first_hit_event_by_trigger[trigger_position] = std::min(
          first_hit_event_by_trigger[trigger_position], hit.event_index);
      last_hit_event_by_trigger[trigger_position] = std::max(
          last_hit_event_by_trigger[trigger_position], hit.event_index);
      if (hit.event_index == best->event_index) {
        ++matched_same_packet;
      } else if (hit.event_index > best->event_index) {
        ++trigger_arrived_before_hit;
        ++matched_cross_packet;
      } else {
        ++hit_arrived_before_trigger;
        ++matched_cross_packet;
      }
    } else {
      ++outside_window;
      if (target_before_trigger_coverage) {
        ++unmatched_before_trigger_coverage;
      } else if (target_after_trigger_coverage) {
        ++unmatched_after_trigger_coverage;
      } else {
        ++unmatched_inside_trigger_coverage;
      }
    }

    hit_matches.push_back(HitMatchRecord{
        trigger_position,
        target_trigger_ns,
        hit_minus_trigger_ns,
        residual_ns,
        accepted,
        target_before_trigger_coverage
            ? "before"
            : (target_after_trigger_coverage ? "after" : "inside"),
        hit.event_index - best->event_index});

    if (examples_printed < 12 &&
        (!accepted || hit.event_index != best->event_index)) {
      std::cout << "  " << (accepted ? "cross-packet match" : "unmatched")
                << ": hit_event=" << hit.event_index
                << " hit=" << hit.hit_index
                << " FEB=" << hit.feb
                << " sampic=" << hit.sampic
                << " channel=" << hit.channel
                << " trigger_event=" << best->event_index
                << " trigger=" << best->trigger_index
                << " fpga_id=" << best->fpga_id
                << " hit-trigger=" << std::fixed << std::setprecision(3)
                << hit_minus_trigger_ns
                << " ns residual_from_offset=" << residual_ns
                << " ns\n" << std::defaultfloat;
      ++examples_printed;
    }
  }

  const std::size_t triggers_with_hits =
      static_cast<std::size_t>(std::count_if(
          hits_per_trigger.begin(),
          hits_per_trigger.end(),
          [](std::size_t count) { return count > 0; }));
  const auto first_populated_trigger = std::find_if(
      hits_per_trigger.begin(),
      hits_per_trigger.end(),
      [](std::size_t count) { return count > 0; });
  const auto last_populated_trigger = std::find_if(
      hits_per_trigger.rbegin(),
      hits_per_trigger.rend(),
      [](std::size_t count) { return count > 0; });
  std::size_t leading_empty_triggers = 0;
  std::size_t trailing_empty_triggers = 0;
  std::size_t internal_empty_triggers = 0;
  std::size_t longest_internal_empty_run = 0;
  std::size_t minimum_hits_per_populated_trigger =
      std::numeric_limits<std::size_t>::max();
  std::size_t maximum_hits_per_populated_trigger = 0;
  if (triggers_with_hits > 0) {
    leading_empty_triggers = static_cast<std::size_t>(
        std::distance(hits_per_trigger.begin(), first_populated_trigger));
    trailing_empty_triggers = static_cast<std::size_t>(
        std::distance(hits_per_trigger.rbegin(), last_populated_trigger));
    const std::size_t last_populated_index =
        hits_per_trigger.size() - trailing_empty_triggers - 1;
    std::size_t current_empty_run = 0;
    for (std::size_t index = leading_empty_triggers;
         index <= last_populated_index;
         ++index) {
      const std::size_t count = hits_per_trigger[index];
      if (count == 0) {
        ++internal_empty_triggers;
        ++current_empty_run;
        longest_internal_empty_run =
            std::max(longest_internal_empty_run, current_empty_run);
      } else {
        minimum_hits_per_populated_trigger =
            std::min(minimum_hits_per_populated_trigger, count);
        maximum_hits_per_populated_trigger =
            std::max(maximum_hits_per_populated_trigger, count);
        current_empty_run = 0;
      }
    }
  }

  std::vector<double> trigger_intervals_ns;
  trigger_intervals_ns.reserve(triggers.size() - 1);
  for (std::size_t index = 1; index < triggers.size(); ++index) {
    trigger_intervals_ns.push_back(
        triggers[index].timestamp_ns - triggers[index - 1].timestamp_ns);
  }
  std::sort(trigger_intervals_ns.begin(), trigger_intervals_ns.end());
  const auto interval_percentile = [&](double fraction) {
    if (trigger_intervals_ns.empty()) {
      return 0.0;
    }
    const std::size_t index = static_cast<std::size_t>(
        fraction * static_cast<double>(trigger_intervals_ns.size() - 1));
    return trigger_intervals_ns[index];
  };
  const double match_percent =
      100.0 * static_cast<double>(matched) /
      static_cast<double>(hits.size());
  std::size_t duplicate_trigger_timestamps = 0;
  for (std::size_t index = 1; index < triggers.size(); ++index) {
    if (std::abs(
            triggers[index].timestamp_ns -
            triggers[index - 1].timestamp_ns) < 1e-6) {
      ++duplicate_trigger_timestamps;
    }
  }

  std::cout << "Raw hits:                         " << hits.size() << "\n"
            << "Raw trigger records:              " << triggers.size() << "\n"
            << "Matched to nearest trigger:       " << matched
            << " (" << std::fixed << std::setprecision(2)
            << match_percent << "%)\n"
            << "  matched in same vendor event:   " << matched_same_packet << "\n"
            << "  matched across vendor events:   " << matched_cross_packet << "\n"
            << "    trigger packet arrived first: " << trigger_arrived_before_hit << "\n"
            << "    hit packet arrived first:     " << hit_arrived_before_trigger << "\n"
            << "Dropped outside time window:      " << outside_window << "\n"
            << "  target before trigger coverage: "
            << unmatched_before_trigger_coverage << "\n"
            << "  target after trigger coverage:  "
            << unmatched_after_trigger_coverage << "\n"
            << "  target inside trigger coverage: "
            << unmatched_inside_trigger_coverage << "\n"
            << "Trigger records with >=1 hit:     " << triggers_with_hits << "\n"
            << "Trigger records without hits:     "
            << (triggers.size() - triggers_with_hits) << "\n"
            << "Nearest residual from configured offset: mean="
            << (nearest_residual_sum / static_cast<double>(hits.size()))
            << " ns, range=[" << nearest_residual_min << ", "
            << nearest_residual_max << "] ns\n"
            << std::defaultfloat;

  std::cout << "Trigger occupancy layout:\n";
  if (triggers_with_hits == 0) {
    std::cout << "  no captured trigger received a hit\n";
  } else {
    std::cout
        << "  empty triggers leading/internal/trailing: "
        << leading_empty_triggers << "/" << internal_empty_triggers << "/"
        << trailing_empty_triggers << "\n"
        << "  longest empty run between populated triggers: "
        << longest_internal_empty_run << "\n"
        << "  hits per populated trigger: mean="
        << (static_cast<double>(matched) /
            static_cast<double>(triggers_with_hits))
        << ", range=[" << minimum_hits_per_populated_trigger << ", "
        << maximum_hits_per_populated_trigger << "]\n";
  }
  if (!trigger_intervals_ns.empty()) {
    const double mean_trigger_interval_ns =
        (triggers.back().timestamp_ns - triggers.front().timestamp_ns) /
        static_cast<double>(trigger_intervals_ns.size());
    std::cout << "  external-trigger interval: mean="
              << mean_trigger_interval_ns
              << " ns, p50=" << interval_percentile(0.50)
              << " ns, p95=" << interval_percentile(0.95)
              << " ns, max=" << trigger_intervals_ns.back() << " ns\n"
              << "  timestamp-derived trigger rate: "
              << (mean_trigger_interval_ns > 0.0
                      ? 1.0e9 / mean_trigger_interval_ns
                      : 0.0)
              << " Hz\n";
  }

  std::cout << "Captured timestamp coverage:\n"
            << "  trigger range: [" << triggers.front().timestamp_ns << ", "
            << triggers.back().timestamp_ns << "] ns (span "
            << (triggers.back().timestamp_ns - triggers.front().timestamp_ns)
            << " ns)\n"
            << "  hit target range: [" << target_timestamp_min << ", "
            << target_timestamp_max << "] ns\n"
            << "  hit targets before/inside/after trigger range: "
            << targets_before_trigger_coverage << "/"
            << targets_inside_trigger_coverage << "/"
            << targets_after_trigger_coverage << "\n";
  if (targets_inside_trigger_coverage > 0) {
    std::cout << "  in-coverage nearest residual: mean="
              << (covered_residual_sum /
                  static_cast<double>(targets_inside_trigger_coverage))
              << " ns, range=[" << covered_residual_min << ", "
              << covered_residual_max << "] ns\n";
  }

  std::cout
      << "\nVendor-event lag (hit event - trigger event), weighted by hits:\n";
  if (options.summary_only) {
    if (vendor_event_lag_histogram.empty()) {
      std::cout << "  no matched packet pairs\n";
    } else {
      const auto most_common = std::max_element(
          vendor_event_lag_histogram.begin(),
          vendor_event_lag_histogram.end(),
          [](const auto& left, const auto& right) {
            return left.second < right.second;
          });
      std::cout << "  range=[" << std::showpos
                << vendor_event_lag_histogram.begin()->first << ", "
                << vendor_event_lag_histogram.rbegin()->first
                << "], mode=" << most_common->first << std::noshowpos
                << " (" << most_common->second << " hit(s))\n";
    }
  } else {
    for (const auto& [lag, count] : vendor_event_lag_histogram) {
      std::cout << "  " << std::showpos << lag << std::noshowpos << ": "
                << count << " hit(s)\n";
    }
  }
  std::cout << "Unique matched hit-packet/trigger-packet pairs: "
            << matched_packet_pairs.size() << "\n";

  std::cout << "\nArrival-order diagnostics:\n"
            << "  hit-bearing vendor packets:     "
            << hit_packet_timestamps.size() << "\n"
            << "  hit packet timestamp inversions:"
            << " " << hit_packet_timestamp_inversions
            << " (largest backward jump "
            << largest_hit_packet_backward_jump_ns << " ns)\n"
            << "  hit timestamp span per packet:  mean="
            << mean_hit_packet_span_ns << " ns, max="
            << maximum_hit_packet_span_ns << " ns\n"
            << "  trigger timestamp inversions:   "
            << trigger_timestamp_inversions
            << " (largest backward jump "
            << largest_trigger_backward_jump_ns << " ns)\n"
            << "  duplicate trigger timestamps:   "
            << duplicate_trigger_timestamps << "\n";

  write_diagnostic_export(
      options,
      decoded_events,
      events_before_cutoff,
      vendor_transport_counters_available,
      lost_transport_frames,
      transport_bytes,
      decoded_readout_bytes,
      decoded_readout_frames,
      capture_wall_s,
      hits,
      triggers,
      hit_matches,
      hits_per_trigger,
      first_hit_event_by_trigger,
      last_hit_event_by_trigger);

  if (!options.summary_only) {
    std::cout << "\nPer-trigger packet layout (triggers receiving hits):\n";
    for (std::size_t index = 0; index < triggers.size(); ++index) {
      if (hits_per_trigger[index] == 0) {
        continue;
      }
      const auto& trigger = triggers[index];
      std::cout << "  fpga_id=" << trigger.fpga_id
                << " trigger_event=" << trigger.event_index
                << " assigned_hits=" << hits_per_trigger[index]
                << " hit_event_range=["
                << first_hit_event_by_trigger[index] << ", "
                << last_hit_event_by_trigger[index] << "]"
                << " lag_range=["
                << std::showpos
                << (first_hit_event_by_trigger[index] - trigger.event_index)
                << ", "
                << (last_hit_event_by_trigger[index] - trigger.event_index)
                << std::noshowpos << "]\n";
    }
  }
}

void configure_batching_session_static_settings(
    SimpleSession& session,
    const DoublePulseConfig& hardware_config,
    const sampic::batching_scan::BatchingScanConfig& scan_config) {
  session.enable_all_channels();
  session.enable_l2_external_gate(
      true,
      hardware_config.scan.board_index,
      hardware_config.scan.channels,
      scan_config.primitive_gate_clocks,
      scan_config.latency_gate_clocks,
      scan_config.external_gate_clocks);
}

void capture_batching_point(
    Options& options,
    const DoublePulseConfig& hardware_config,
    SimpleSession& session,
    sampic::lecroy::LecroyClient& lecroy,
    sampic::lecroy::LecroyOutputGate& lecroy_output) {
  std::cout << "Entered capture_batching_point." << std::endl;
  const int rate_mhz = hardware_config.scan.digitizer_rates_mhz.empty()
                           ? 6400
                           : hardware_config.scan.digitizer_rates_mhz.front();
  const double sample_period_ns = 1000.0 / static_cast<double>(rate_mhz);

  std::cout << "Capture setup: inhibiting Lecroy output." << std::endl;

  try {
    lecroy_output.Disable();
  } catch (const std::exception& error) {
    throw sampic::tests::ProbeFatalError(
        "Unable to inhibit the Lecroy output before acquisition: " +
        std::string(error.what()));
  }
  std::cout << "Capture setup: Lecroy output inhibited; programming "
               "frequency."
            << std::endl;
  try {
    lecroy.SetFrequency(*options.lecroy_rate_hz);
    options.lecroy_rate_readback_hz = std::stod(lecroy.Query("FREQ?"));
  } catch (const std::exception& error) {
    throw std::runtime_error(
        "Unable to program or verify the Lecroy frequency: " +
        std::string(error.what()));
  }
  {
    const auto previous_flags = std::cout.flags();
    const auto previous_precision = std::cout.precision();
    std::cout << "Lecroy frequency readback: "
              << std::defaultfloat << std::setprecision(12)
              << *options.lecroy_rate_readback_hz << " Hz (requested "
              << *options.lecroy_rate_hz << " Hz)" << std::endl;
    std::cout.flags(previous_flags);
    std::cout.precision(previous_precision);
  }

  const bool vendor_transport_counters_available =
      LPD_ResetLostFrames != nullptr && LPD_GetLostFrames != nullptr &&
      LPD_ResetTotalByteCount != nullptr && LPD_GetTotalByteCount != nullptr;
  if (vendor_transport_counters_available) {
    std::cout << "Capture setup: resetting vendor transport counters."
              << std::endl;
    LPD_ResetLostFrames();
    LPD_ResetTotalByteCount();
    std::cout << "Capture setup: vendor transport counters reset."
              << std::endl;
  } else {
    std::cout << "Capture setup: vendor transport counters unavailable; "
                 "skipping reset."
              << std::endl;
  }

  bool run_started = false;
  try {
    std::cout << "Capture setup: calling SAMPIC256CH_StartRun." << std::endl;
    if (!session.start_run(hardware_config.start_retry)) {
      throw std::runtime_error(
          "Failed to start the SAMPIC run after retry attempts");
    }
    run_started = true;
    std::cout << "Capture setup: SAMPIC run started; enabling Lecroy output."
              << std::endl;
    try {
      lecroy_output.Enable();
    } catch (const std::exception& error) {
      throw sampic::tests::ProbeFatalError(
          "Unable to enable the Lecroy output for acquisition: " +
          std::string(error.what()));
    }
    std::cout << "Capture setup complete; acquisition active." << std::endl;

    // EventStruct contains MAX_EXPECTED_FRAMES full HitStruct objects and is
    // about 7.3 MiB with this vendor library. Keeping it local made this
    // function's stack frame exceed the default 8 MiB thread stack before its
    // first statement could run.
    auto event = std::make_unique<EventStruct>();
    const auto capture_start = std::chrono::steady_clock::now();
    int events_printed = 0;
    std::vector<ObservedHit> observed_hits;
    std::vector<ObservedTrigger> observed_triggers;
    if (options.lecroy_rate_readback_hz) {
      const auto expected_triggers = static_cast<std::size_t>(std::ceil(
          *options.lecroy_rate_readback_hz * options.max_duration_s * 1.10));
      observed_triggers.reserve(expected_triggers);
      observed_hits.reserve(expected_triggers * 8);
    }
    std::uint64_t decoded_readout_bytes = 0;
    std::uint64_t decoded_readout_frames = 0;
    options.collection_timings.clear();
    options.collection_timings.reserve(
        static_cast<std::size_t>(options.max_events));
    std::optional<std::chrono::steady_clock::time_point>
        previous_successful_read;

    auto record_event = [&](EventStruct& decoded_event,
                            int event_index,
                            int frames,
                            int bytes) {
      decoded_readout_bytes +=
          static_cast<std::uint64_t>(std::max(0, bytes));
      decoded_readout_frames +=
          static_cast<std::uint64_t>(std::max(0, frames));
      for (int hit_index = 0; hit_index < decoded_event.NbOfHitsInEvent;
           ++hit_index) {
        const auto& hit = decoded_event.Hit[hit_index];
        observed_hits.push_back(ObservedHit{
            event_index,
            hit_index,
            hit.FeBoardIndex,
            hit.SampicIndex,
            hit.Channel,
            hit.FirstCellTimeStamp +
                hit.AdvancedParams.FirstTriggerPositionCell *
                    sample_period_ns,
            hit.FirstCellTimeStamp,
            hit.AdvancedParams.FirstTriggerPositionCell});
      }
      for (int trigger_index = 0;
           trigger_index < decoded_event.TriggerData.NbOfTriggers;
           ++trigger_index) {
        observed_triggers.push_back(ObservedTrigger{
            event_index,
            trigger_index,
            decoded_event.TriggerData.TriggerIDFromFPGA[trigger_index],
            decoded_event.TriggerData.TriggerIDFromExtTrig[trigger_index],
            decoded_event.TriggerData.TriggerTimeStamp[trigger_index]});
      }
    };

    auto disable_generator_at_cutoff = [&]() {
      try {
        lecroy_output.Disable();
      } catch (const std::exception& error) {
        throw sampic::tests::ProbeFatalError(
            "Unable to inhibit the Lecroy output at capture cutoff: " +
            std::string(error.what()));
      }
    };

    ReadoutConfig drain_readout = hardware_config.readout;
    drain_readout.retry_sleep_us =
        std::max(100, drain_readout.retry_sleep_us);
    drain_readout.max_loops = std::max(
        1,
        static_cast<int>(std::ceil(
            options.drain_quiet_ms * 1000.0 /
            static_cast<double>(drain_readout.retry_sleep_us))));

    int events_at_cutoff = 0;
    int drained_events = 0;
    if (options.pipelined_decode) {
      std::cout << "Pipelined acquisition: receiver -> bounded raw-frame "
                   "queue -> decoder (capacity="
                << options.raw_queue_capacity << ")\n";
      RawVendorEventQueue raw_queue(options.raw_queue_capacity);
      auto decoder_info = std::make_unique<CrateInfoStruct>();
      auto decoder_params = std::make_unique<CrateParamStruct>();
      session.copy_decoder_context(*decoder_info, *decoder_params);
      std::exception_ptr decoder_error;
      std::atomic<bool> decoder_failed{false};

      std::thread decoder([&]() {
        try {
          auto decoded_event = std::make_unique<EventStruct>();
          while (auto* raw = raw_queue.pop()) {
            int hits = 0;
            if (!SimpleSession::decode_raw_event(
                    *decoder_info,
                    *decoder_params,
                    *raw,
                    *decoded_event,
                    hits)) {
              throw std::runtime_error("SAMPIC256CH_DecodeEvent failed");
            }
            const auto processing_start = std::chrono::steady_clock::now();
            record_event(
                *decoded_event,
                raw->event_index,
                static_cast<int>(raw->frames.size()),
                raw->bytes);
            const double processing_us =
                std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - processing_start)
                    .count();
            options.collection_timings.push_back(CollectionTimingRecord{
                raw->event_index,
                raw->during_drain,
                hits,
                static_cast<int>(raw->frames.size()),
                raw->bytes,
                raw->vendor_timing,
                raw->read_call_us,
                processing_us,
                raw->successful_read_gap_us});
            raw_queue.release(raw);
          }
        } catch (...) {
          decoder_error = std::current_exception();
          decoder_failed = true;
          raw_queue.close();
        }
      });

      try {
        int received_events = 0;
        auto receive_one = [&](const ReadoutConfig& readout,
                             bool during_drain,
                             bool report_timeout) {
        RawVendorEvent* raw = raw_queue.acquire();
        if (!raw) return false;
        const auto read_start = std::chrono::steady_clock::now();
        if (!session.read_raw_event(readout, *raw, report_timeout)) {
          raw_queue.release(raw);
          return false;
        }
        const auto read_complete = std::chrono::steady_clock::now();
        raw->read_call_us = std::chrono::duration<double, std::micro>(
                                read_complete - read_start)
                                .count();
        raw->successful_read_gap_us =
            previous_successful_read
                ? std::chrono::duration<double, std::micro>(
                      read_complete - *previous_successful_read)
                      .count()
                : 0.0;
        previous_successful_read = read_complete;
        raw->event_index = ++received_events;
        raw->during_drain = during_drain;
        raw_queue.publish(raw);
        return true;
        };

        while (received_events < options.max_events && !decoder_failed) {
          const double elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() -
                                     capture_start)
                                     .count();
          if (elapsed >= options.max_duration_s) break;
          receive_one(hardware_config.readout, false, true);
        }
        options.excitation_wall_s = std::chrono::duration<double>(
                                        std::chrono::steady_clock::now() -
                                        capture_start)
                                        .count();
        events_at_cutoff = received_events;
        disable_generator_at_cutoff();

        const auto drain_deadline =
            std::chrono::steady_clock::now() +
            std::chrono::duration<double>(options.drain_timeout_s);
        while (std::chrono::steady_clock::now() < drain_deadline &&
               !decoder_failed &&
               receive_one(drain_readout, true, false)) {
        }
        drained_events = received_events - events_at_cutoff;
        raw_queue.close();
        decoder.join();
        options.raw_queue_high_water_mark = raw_queue.high_water_mark();
        options.raw_queue_producer_wait_us = raw_queue.producer_wait_us();
        if (decoder_error) std::rethrow_exception(decoder_error);
        events_printed = received_events;
      } catch (...) {
        raw_queue.close();
        if (decoder.joinable()) decoder.join();
        throw;
      }
    } else {
      while (events_printed < options.max_events) {
      const double elapsed = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() -
                                 capture_start)
                                 .count();
      if (elapsed >= options.max_duration_s) break;
      int hits = 0;
      int frames = 0;
      int bytes = 0;
      // DecodeEvent resets the event counts and every populated HitStruct;
      // the vendor examples reuse EventStruct without clearing all 7.3 MiB.
      ReadEventTiming vendor_timing;
      const auto read_start = std::chrono::steady_clock::now();
      if (session.read_event(
              hardware_config.readout,
              *event,
              hits,
              frames,
              bytes,
              true,
              &vendor_timing)) {
        const auto read_complete = std::chrono::steady_clock::now();
        const double read_call_us =
            std::chrono::duration<double, std::micro>(
                read_complete - read_start)
                .count();
        const double successful_read_gap_us = previous_successful_read
                                                  ? std::chrono::duration<
                                                        double,
                                                        std::micro>(
                                                        read_complete -
                                                        *previous_successful_read)
                                                        .count()
                                                  : 0.0;
        previous_successful_read = read_complete;
        const auto processing_start = std::chrono::steady_clock::now();
        ++events_printed;
        record_event(*event, events_printed, frames, bytes);
        const double processing_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - processing_start)
                .count();
        options.collection_timings.push_back(CollectionTimingRecord{
            events_printed,
            false,
            hits,
            frames,
            bytes,
            vendor_timing,
            read_call_us,
            processing_us,
            successful_read_gap_us});
      }
      }

      options.excitation_wall_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() -
                                      capture_start)
                                      .count();
      events_at_cutoff = events_printed;
      disable_generator_at_cutoff();

      const auto drain_deadline =
          std::chrono::steady_clock::now() +
          std::chrono::duration<double>(options.drain_timeout_s);
      while (std::chrono::steady_clock::now() < drain_deadline) {
      int hits = 0;
      int frames = 0;
      int bytes = 0;
      ReadEventTiming vendor_timing;
      const auto read_start = std::chrono::steady_clock::now();
      if (!session.read_event(
              drain_readout,
              *event,
              hits,
              frames,
              bytes,
              false,
              &vendor_timing)) {
        break;
      }
      const auto read_complete = std::chrono::steady_clock::now();
      const double read_call_us = std::chrono::duration<double, std::micro>(
                                      read_complete - read_start)
                                      .count();
      const double successful_read_gap_us = previous_successful_read
                                                ? std::chrono::duration<
                                                      double,
                                                      std::micro>(
                                                      read_complete -
                                                      *previous_successful_read)
                                                      .count()
                                                : 0.0;
      previous_successful_read = read_complete;
      const auto processing_start = std::chrono::steady_clock::now();
      ++events_printed;
      record_event(*event, events_printed, frames, bytes);
      const double processing_us = std::chrono::duration<double, std::micro>(
                                       std::chrono::steady_clock::now() -
                                       processing_start)
                                       .count();
      options.collection_timings.push_back(CollectionTimingRecord{
          events_printed,
          true,
          hits,
          frames,
          bytes,
          vendor_timing,
          read_call_us,
          processing_us,
          successful_read_gap_us});
        ++drained_events;
      }
    }

    const double capture_wall_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() -
                                      capture_start)
                                      .count();
    double active_read_us = 0.0;
    double active_processing_us = 0.0;
    double maximum_processing_us = 0.0;
    std::uint64_t active_read_buffer_calls = 0;
    std::size_t active_timing_records = 0;
    for (const auto& timing : options.collection_timings) {
      if (timing.during_drain) continue;
      ++active_timing_records;
      active_read_us += timing.read_call_us;
      active_processing_us += timing.processing_us;
      maximum_processing_us =
          std::max(maximum_processing_us, timing.processing_us);
      active_read_buffer_calls += timing.vendor.read_buffer_calls;
    }
    const double active_wall_us = options.excitation_wall_s * 1.0e6;
    std::cout
        << "Collection-loop timing before cutoff:\n"
        << "  acquisition model: "
        << (options.pipelined_decode ? "pipelined receive/decode"
                                     : "synchronous")
        << "\n"
        << "  successful vendor events: " << active_timing_records << "\n"
        << "  vendor read / compact-copy processing: "
        << active_read_us / 1.0e6 << " / "
        << active_processing_us / 1.0e6 << " s\n"
        << "  compact-copy fraction of active wall time: "
        << (active_wall_us > 0.0
                ? 100.0 * active_processing_us / active_wall_us
                : 0.0)
        << "%\n"
        << "  vendor ReadEventBuffer calls per successful event: "
        << (active_timing_records > 0
                ? static_cast<double>(active_read_buffer_calls) /
                      static_cast<double>(active_timing_records)
                : 0.0)
        << "\n"
        << "  mean / maximum post-read processing: "
        << (active_timing_records > 0
                ? active_processing_us /
                      static_cast<double>(active_timing_records)
                : 0.0)
        << " / " << maximum_processing_us << " us\n";
    if (options.pipelined_decode) {
      std::cout << "  raw queue high-water mark: "
                << options.raw_queue_high_water_mark << " / "
                << options.raw_queue_capacity << "\n"
                << "  receiver wait for free queue slots: "
                << options.raw_queue_producer_wait_us / 1.0e6 << " s\n";
    }
    const unsigned long lost_transport_frames =
        vendor_transport_counters_available ? LPD_GetLostFrames() : 0;
    const unsigned long transport_bytes =
        vendor_transport_counters_available ? LPD_GetTotalByteCount() : 0;
    session.stop_run();
    run_started = false;

    std::cout << "Captured " << events_at_cutoff
              << " event(s) before cutoff and " << drained_events
              << " queued event(s) during drain (" << events_printed
              << " total).\n";

    Options correlation_options = options;
    if (options.auto_hit_offset) {
      const auto estimated_offset =
          estimate_hit_time_offset_ns(observed_hits, observed_triggers);
      if (estimated_offset) {
        correlation_options.hit_time_offset_ns = *estimated_offset;
        std::cout << "Offline hit-offset estimate: " << *estimated_offset
                  << " ns\n";
      }
    }
    const auto same_event_stats = summarize_same_vendor_event_association(
        correlation_options, observed_hits, observed_triggers);
    print_summary(events_printed, same_event_stats);
    print_stream_correlation(
        correlation_options,
        events_printed,
        events_at_cutoff,
        vendor_transport_counters_available,
        lost_transport_frames,
        transport_bytes,
        decoded_readout_bytes,
        decoded_readout_frames,
        capture_wall_s,
        std::move(observed_hits),
        std::move(observed_triggers));

    if (!std::filesystem::exists(
            std::filesystem::path(options.output_dir) / "metadata.json")) {
      throw std::runtime_error(
          "Point completed without a diagnostic metadata export");
    }
  } catch (...) {
    try {
      lecroy_output.Disable();
    } catch (const std::exception& error) {
      throw sampic::tests::ProbeFatalError(
          "Unable to inhibit the Lecroy output during point cleanup: " +
          std::string(error.what()));
    }
    if (run_started) session.stop_run();
    throw;
  }
}

struct BatchingScanCommand {
  std::filesystem::path config_path;
  std::optional<std::filesystem::path> output_dir;
  bool resume = false;
  bool dry_run = false;
};

BatchingScanCommand parse_batching_scan_command(int argc, char** argv) {
  BatchingScanCommand command;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    auto value = [&]() -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error(std::string(argument) + " requires a value");
      }
      return argv[++index];
    };
    if (argument == "--batch-scan-config") {
      command.config_path = value();
    } else if (argument == "--output-dir") {
      command.output_dir = value();
    } else if (argument == "--resume") {
      command.resume = true;
    } else if (argument == "--dry-run") {
      command.dry_run = true;
    } else {
      throw std::runtime_error(
          "Unknown batching-scan option: " + std::string(argument));
    }
  }
  if (command.config_path.empty()) {
    throw std::runtime_error("--batch-scan-config is required");
  }
  return command;
}

std::filesystem::path batching_project_dir() {
  const auto executable = std::filesystem::canonical("/proc/self/exe");
  return std::filesystem::weakly_canonical(
      executable.parent_path() / "../..");
}

std::string timestamp_directory_name() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream output;
  output << std::put_time(&local, "%Y%m%d_%H%M%S");
  return output.str();
}

std::filesystem::path latest_scan_directory(
    const std::filesystem::path& output_root) {
  std::filesystem::path latest;
  if (std::filesystem::exists(output_root)) {
    for (const auto& entry : std::filesystem::directory_iterator(output_root)) {
      if (!entry.is_directory() ||
          !std::filesystem::exists(entry.path() / "scan_manifest.csv")) {
        continue;
      }
      if (latest.empty() ||
          entry.path().filename().string() > latest.filename().string()) {
        latest = entry.path();
      }
    }
  }
  if (latest.empty()) {
    throw std::runtime_error(
        "--resume requested without --output-dir, but no prior scan exists in " +
        output_root.string());
  }
  return latest;
}

void append_manifest_row(
    const std::filesystem::path& manifest,
    const sampic::batching_scan::BatchingScanPoint& point,
    std::string_view status,
    const std::string& run_name) {
  std::ofstream output(manifest, std::ios::app);
  if (!output) {
    throw std::runtime_error("Unable to append scan manifest");
  }
  output << point.rate_hz << ',' << point.frames_per_block << ','
         << point.triggers_per_event << ',' << point.repetition << ','
         << status << ',' << run_name << '\n';
}

int run_persistent_batching_scan(int argc, char** argv) {
  const auto command = parse_batching_scan_command(argc, argv);
  const auto project_dir = batching_project_dir();
  const auto scan_config = sampic::batching_scan::load_batching_scan_config(
      command.config_path, project_dir);
  const auto points =
      sampic::batching_scan::build_batching_scan_points(scan_config);
  const auto output_dir = command.output_dir
                              ? std::filesystem::absolute(*command.output_dir)
                          : command.resume
                              ? latest_scan_directory(scan_config.output_root)
                              : scan_config.output_root /
                                    timestamp_directory_name();

  std::cout << "Persistent batching scan: " << points.size() << " point(s)\n"
            << "  scan config: " << std::filesystem::absolute(command.config_path)
            << "\n  hardware config: " << scan_config.hardware_config
            << "\n  output: " << output_dir << "\n"
            << "  crate policy: one persistent connection; reconnect only "
               "after point failure\n";
  if (command.dry_run) return 0;

  std::filesystem::create_directories(output_dir);
  const auto manifest = output_dir / "scan_manifest.csv";
  if (!std::filesystem::exists(manifest)) {
    std::ofstream output(manifest);
    output << "requested_rate_hz,frames_per_block,triggers_per_event,"
              "repetition,status,run_dir\n";
  }
  {
    std::ofstream output(output_dir / "scan_config.env");
    output << "source_config=" << std::filesystem::absolute(command.config_path)
           << '\n';
    output << "persistent_crate_connection=true\n";
    output << "frames=";
    for (std::size_t index = 0; index < scan_config.frames_per_block.size(); ++index) {
      if (index) output << ',';
      output << scan_config.frames_per_block[index];
    }
    output << "\ntriggers=";
    for (std::size_t index = 0; index < scan_config.triggers_per_event.size(); ++index) {
      if (index) output << ',';
      output << scan_config.triggers_per_event[index];
    }
    output << '\n';
    output << "rates_hz=";
    for (std::size_t index = 0; index < scan_config.lecroy_rates_hz.size(); ++index) {
      if (index) output << ',';
      output << scan_config.lecroy_rates_hz[index];
    }
    output << "\nduration_s=" << scan_config.duration_s
           << "\nmax_events=" << scan_config.max_events
           << "\npipelined_decode="
           << (scan_config.pipelined_decode ? "true" : "false")
           << "\nraw_queue_capacity=" << scan_config.raw_queue_capacity
           << "\nrepetitions=" << scan_config.repetitions
           << "\nmax_point_retries=" << scan_config.max_point_retries
           << "\nretry_delay_s=" << scan_config.retry_delay_s
           << "\nmtu_bytes=1500\nsingle_frame_bytes=160\n"
              "largest_unfragmented_frames_per_block=8\n";
  }

  const auto hardware_config =
      sampic::double_pulse::load_double_pulse_config(
          scan_config.hardware_config.string());
  sampic::lecroy::LecroyClient lecroy;
  lecroy.Connect(hardware_config.lecroy.ip, hardware_config.lecroy.port);
  sampic::lecroy::LecroyOutputGate lecroy_output(lecroy, {"B"});
  try {
    lecroy_output.Disable();
  } catch (const std::exception& error) {
    throw sampic::tests::ProbeFatalError(
        "Unable to establish safe Lecroy output state: " +
        std::string(error.what()));
  }

  const int sampling_rate_mhz =
      hardware_config.scan.digitizer_rates_mhz.empty()
          ? 6400
          : hardware_config.scan.digitizer_rates_mhz.front();
  std::unique_ptr<SimpleSession> session;
  bool session_needs_static_settings = false;
  auto ensure_session = [&]() -> SimpleSession& {
    if (!session) {
      session = std::make_unique<SimpleSession>(
          hardware_config.connection,
          hardware_config.external_trigger,
          true);
      session->set_sampling_rate(sampling_rate_mhz);
      session_needs_static_settings = true;
      std::cout << "Persistent crate connection initialized." << std::endl;
    }
    return *session;
  };

  std::signal(SIGINT, batching_scan_signal_handler);
  std::signal(SIGTERM, batching_scan_signal_handler);
  std::size_t failed_points = 0;
  const auto scan_start = std::chrono::steady_clock::now();

  for (std::size_t point_index = 0; point_index < points.size(); ++point_index) {
    const auto& point = points[point_index];
    const auto run_name = sampic::batching_scan::batching_scan_run_name(point);
    const auto run_dir = output_dir / run_name;
    if (command.resume &&
        std::filesystem::exists(run_dir / "metadata.json")) {
      std::cout << '[' << point_index + 1 << '/' << points.size()
                << "] Reusing " << run_name << '\n';
      append_manifest_row(manifest, point, "reused", run_name);
      continue;
    }
    if (g_stop_after_current_point) break;

    std::filesystem::create_directories(run_dir);
    std::cout << "\n[" << point_index + 1 << '/' << points.size() << "] "
              << run_name << '\n';
    bool complete = false;
    int attempts_used = 0;
    std::string last_error;
    for (int attempt = 1;
         attempt <= scan_config.max_point_retries + 1;
         ++attempt) {
      attempts_used = attempt;
      try {
        std::cout << "Preparing point options." << std::endl;
        Options options;
        options.config_path = scan_config.hardware_config.string();
        options.output_dir = run_dir.string();
        options.max_events = scan_config.max_events;
        options.max_duration_s = scan_config.duration_s;
        options.pipelined_decode = scan_config.pipelined_decode;
        options.raw_queue_capacity = scan_config.raw_queue_capacity;
        options.use_self_trigger_channels = true;
        options.use_l2_external_gate = true;
        options.skip_lecroy = true;
        options.manage_lecroy_output = true;
        options.lecroy_output_channel = "B";
        options.lecroy_rate_hz = point.rate_hz;
        options.summary_only = true;
        options.all_channels = true;
        options.persistent_session = true;
        options.drain_quiet_ms = scan_config.drain_quiet_ms;
        options.drain_timeout_s = scan_config.drain_timeout_s;
        options.primitive_gate_clocks = scan_config.primitive_gate_clocks;
        options.latency_gate_clocks = scan_config.latency_gate_clocks;
        options.external_gate_clocks = scan_config.external_gate_clocks;
        options.frames_per_block = point.frames_per_block;
        options.triggers_per_event = point.triggers_per_event;
        options.hit_time_offset_ns = scan_config.hit_offset_ns;
        options.requested_hit_time_offset_ns = scan_config.hit_offset_ns;
        options.auto_hit_offset = scan_config.auto_hit_offset;
        options.pre_window_ns = scan_config.pre_window_ns;
        options.post_window_ns = scan_config.post_window_ns;
        std::cout << "Point options prepared." << std::endl;
        auto& active_session = ensure_session();
        // Match the known-good single-probe initialization order on every
        // fresh connection: sampling rate, packetization, then channels/L2.
        // StopRun() resets frames-per-block, so packetization is reapplied for
        // every point even while the crate connection remains open.
        std::cout << "Applying transport packetization: frames_per_block="
                  << point.frames_per_block
                  << ", triggers_per_event=" << point.triggers_per_event
                  << std::endl;
        active_session.set_packetization(
            point.frames_per_block, point.triggers_per_event);
        if (session_needs_static_settings) {
          std::cout << "Applying persistent channel and L2 gate settings."
                    << std::endl;
          configure_batching_session_static_settings(
              active_session, hardware_config, scan_config);
          session_needs_static_settings = false;
          std::cout << "Persistent crate session initialized." << std::endl;
        }
        // Some vendor routines install process signal handlers. Restore our
        // diagnostic handlers after hardware configuration so a native fault
        // produces a usable backtrace in the screen log.
        install_crash_signal_handlers();
        std::cout << "Calling capture_batching_point." << std::endl;
        capture_batching_point(
            options,
            hardware_config,
            active_session,
            lecroy,
            lecroy_output);
        complete = true;
        break;
      } catch (const sampic::tests::ProbeFatalError&) {
        append_manifest_row(manifest, point, "fatal", run_name);
        throw;
      } catch (const std::exception& error) {
        last_error = error.what();
        std::cerr << "Point attempt " << attempt << " failed: "
                  << last_error
                  << "\nRefreshing instrument connections before retry.\n";
        session.reset();
        session_needs_static_settings = false;
        if (attempt <= scan_config.max_point_retries) {
          try {
            lecroy.Reconnect();
            lecroy_output.Disable();
          } catch (const std::exception& reconnect_error) {
            throw sampic::tests::ProbeFatalError(
                "Unable to restore safe Lecroy control for retry: " +
                std::string(reconnect_error.what()));
          }
          std::this_thread::sleep_for(
              std::chrono::duration<double>(scan_config.retry_delay_s));
        }
      }
    }

    {
      std::ofstream log(run_dir / "run.log", std::ios::app);
      log << (complete ? "complete" : "failed") << " after "
          << attempts_used << " attempt(s)";
      if (!last_error.empty()) log << ": " << last_error;
      log << '\n';
    }
    if (complete) {
      append_manifest_row(manifest, point, "complete", run_name);
    } else {
      ++failed_points;
      append_manifest_row(manifest, point, "failed", run_name);
    }

    const double elapsed = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - scan_start)
                               .count();
    const double average = elapsed / static_cast<double>(point_index + 1);
    const double remaining = average *
        static_cast<double>(points.size() - point_index - 1);
    {
      const auto previous_flags = std::cout.flags();
      const auto previous_precision = std::cout.precision();
      std::cout << "Progress: " << point_index + 1 << '/' << points.size()
                << "; average " << std::fixed << std::setprecision(1)
                << average << " s/point; ETA " << remaining / 3600.0
                << " h\n";
      std::cout.flags(previous_flags);
      std::cout.precision(previous_precision);
    }
    if (g_stop_after_current_point) break;
  }

  lecroy_output.Disable();
  session.reset();
  std::cout << "Batching scan finished safely. Non-fatal failed points: "
            << failed_points << "\n";
  return failed_points == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  // A disconnected instrument socket must become a recoverable send error,
  // not an unlogged process-killing SIGPIPE.
  std::signal(SIGPIPE, SIG_IGN);
  install_crash_signal_handlers();
  try {
    for (int index = 1; index < argc; ++index) {
      if (std::string_view(argv[index]) == "--batch-scan-config") {
        return run_persistent_batching_scan(argc, argv);
      }
    }
    auto opts = parse_args(argc, argv);
    auto cfg = sampic::double_pulse::load_double_pulse_config(opts.config_path);
    SimpleSession session(
        cfg.connection,
        cfg.external_trigger,
        opts.use_self_trigger_channels);
    std::cout << "SAMPIC channel trigger mode: "
              << (opts.use_self_trigger_channels
                      ? "self trigger"
                      : "external trigger")
              << "\n";
    const int rate_mhz =
        cfg.scan.digitizer_rates_mhz.empty() ? 6400 : cfg.scan.digitizer_rates_mhz.front();
    ExternalTriggerHitAssociator associator(
        opts.hit_time_offset_ns,
        opts.pre_window_ns,
        opts.post_window_ns,
        static_cast<double>(rate_mhz));
    std::cout << "Association settings: hit_offset=" << opts.hit_time_offset_ns
              << " ns, window=[-" << opts.pre_window_ns << ", +"
              << opts.post_window_ns << "] ns, sampling_frequency="
              << rate_mhz << " MHz\n";
    session.set_sampling_rate(rate_mhz);
    session.set_packetization(
        opts.frames_per_block, opts.triggers_per_event);
    std::cout << "Transport packetization: frames_per_block="
              << opts.frames_per_block
              << ", triggers_per_event=" << opts.triggers_per_event << "\n";
    if (opts.all_channels) {
      std::cout << "Enabling all channels on all discovered FEBs.\n";
      session.enable_all_channels();
    } else {
      session.enable_channels(cfg.scan.board_index, cfg.scan.channels);
    }
    if (opts.use_l2_external_gate) {
      session.enable_l2_external_gate(
          opts.all_channels,
          cfg.scan.board_index,
          cfg.scan.channels,
          opts.primitive_gate_clocks,
          opts.latency_gate_clocks,
          opts.external_gate_clocks);
    } else if (opts.use_self_trigger_channels) {
      std::cout
          << "L2 external hardware gate: DISABLED; self-trigger and "
             "external-counter streams are independent.\n";
    }

    std::unique_ptr<sampic::lecroy::LecroyClient> lecroy;
    std::unique_ptr<sampic::lecroy::LecroyOutputGate> lecroy_output;
    std::unique_ptr<sampic::lecroy::ManualTriggerController> manual_trigger;
    if (!opts.skip_lecroy) {
      lecroy = std::make_unique<sampic::lecroy::LecroyClient>();
      lecroy->Configure(cfg.lecroy);
      if (cfg.lecroy.manual_trigger && cfg.lecroy.manual_trigger_interval_s > 0.0) {
        manual_trigger = std::make_unique<sampic::lecroy::ManualTriggerController>(
            lecroy.get(), cfg.lecroy.manual_trigger_interval_s, nullptr);
      }
    } else if (opts.manage_lecroy_output) {
      // Connect without rewriting pulse settings. An explicit rate override is
      // applied below while the selected output is disabled.
      lecroy = std::make_unique<sampic::lecroy::LecroyClient>();
      try {
        lecroy->Connect(cfg.lecroy.ip, cfg.lecroy.port);
      } catch (const std::exception& error) {
        throw std::runtime_error(
            "Unable to connect to the Lecroy generator: " +
            std::string(error.what()));
      }
    } else {
      std::cout << "Skipping Lecroy configuration per user request.\n";
    }

    if (lecroy) {
      std::vector<std::string> channels;
      if (!opts.lecroy_output_channel.empty()) {
        channels.push_back(opts.lecroy_output_channel);
      } else {
        channels = cfg.lecroy.channels;
        if (channels.empty()) {
          channels.push_back(cfg.lecroy.channel.channel);
        }
      }
      lecroy_output =
          std::make_unique<sampic::lecroy::LecroyOutputGate>(
              *lecroy, std::move(channels));
      // Do not generate a leading backlog while StartRun resynchronizes.
      try {
        lecroy_output->Disable();
      } catch (const std::exception& error) {
        throw sampic::tests::ProbeFatalError(
            "Unable to inhibit the Lecroy output before acquisition: " +
            std::string(error.what()));
      }
      std::cout << "Lecroy output disabled while starting SAMPIC acquisition.\n";

      if (opts.lecroy_rate_hz) {
        try {
          lecroy->SetFrequency(*opts.lecroy_rate_hz);
        } catch (const std::exception& error) {
          throw std::runtime_error(
              "Unable to program the Lecroy frequency: " +
              std::string(error.what()));
        }
      }
      try {
        const auto response = lecroy->Query("FREQ?");
        opts.lecroy_rate_readback_hz = std::stod(response);
        std::cout << "Lecroy frequency readback: "
                  << *opts.lecroy_rate_readback_hz << " Hz";
        if (opts.lecroy_rate_hz) {
          std::cout << " (requested " << *opts.lecroy_rate_hz << " Hz)";
        }
        std::cout << "\n";
      } catch (const std::exception& ex) {
        if (opts.lecroy_rate_hz) {
          throw std::runtime_error(
              "Unable to verify the Lecroy frequency: " +
              std::string(ex.what()));
        }
        std::cerr << "Warning: failed to read Lecroy frequency: "
                  << ex.what() << "\n";
      }
    }

    const bool vendor_transport_counters_available =
        LPD_ResetLostFrames != nullptr &&
        LPD_GetLostFrames != nullptr &&
        LPD_ResetTotalByteCount != nullptr &&
        LPD_GetTotalByteCount != nullptr;
    if (vendor_transport_counters_available) {
      LPD_ResetLostFrames();
      LPD_ResetTotalByteCount();
    } else {
      std::cout
          << "Vendor transport counters unavailable in the installed "
             "liblpdevC.so.\n";
    }
    if (!session.start_run(cfg.start_retry)) {
      throw std::runtime_error(
          "Failed to start the SAMPIC run after retry attempts");
    }
    if (lecroy_output) {
      try {
        lecroy_output->Enable();
      } catch (const std::exception& error) {
        throw sampic::tests::ProbeFatalError(
            "Unable to enable the Lecroy output for acquisition: " +
            std::string(error.what()));
      }
      std::cout << "Lecroy output enabled for capture.\n";
    }
    sampic::lecroy::ManualTriggerGuard guard(manual_trigger.get());

    EventStruct event{};
    const auto t_begin = std::chrono::steady_clock::now();
    int events_printed = 0;
    ExternalTriggerAssociationStats total_stats;
    std::vector<ObservedHit> observed_hits;
    std::vector<ObservedTrigger> observed_triggers;
    std::uint64_t decoded_readout_bytes = 0;
    std::uint64_t decoded_readout_frames = 0;
    auto record_event = [&](int hits, int frames, int bytes) {
      ++events_printed;
      decoded_readout_bytes += static_cast<std::uint64_t>(std::max(0, bytes));
      decoded_readout_frames += static_cast<std::uint64_t>(std::max(0, frames));
      const auto association = associator.associate(event);
      add_stats(total_stats, association.stats);
      for (int hit_index = 0; hit_index < event.NbOfHitsInEvent; ++hit_index) {
        const auto& hit = event.Hit[hit_index];
        const auto& decision =
            association.hit_decisions[static_cast<std::size_t>(hit_index)];
        observed_hits.push_back(ObservedHit{
            events_printed,
            hit_index,
            hit.FeBoardIndex,
            hit.SampicIndex,
            hit.Channel,
            decision.hit_timestamp_ns,
            hit.FirstCellTimeStamp,
            hit.AdvancedParams.FirstTriggerPositionCell});
      }
      for (int trigger_index = 0;
           trigger_index < event.TriggerData.NbOfTriggers;
           ++trigger_index) {
        observed_triggers.push_back(ObservedTrigger{
            events_printed,
            trigger_index,
            event.TriggerData.TriggerIDFromFPGA[trigger_index],
            event.TriggerData.TriggerIDFromExtTrig[trigger_index],
            event.TriggerData.TriggerTimeStamp[trigger_index]});
      }
      if (!opts.summary_only) {
        print_event(events_printed, event, hits, frames, bytes, association);
      }
    };

    while (events_printed < opts.max_events) {
      const auto now = std::chrono::steady_clock::now();
      if (opts.max_duration_s > 0.0) {
        const double elapsed = std::chrono::duration<double>(now - t_begin).count();
        if (elapsed >= opts.max_duration_s) break;
      }

      int hits = 0;
      int frames = 0;
      int bytes = 0;
      event = EventStruct{};
      if (!session.read_event(cfg.readout, event, hits, frames, bytes)) {
        continue;
      }
      record_event(hits, frames, bytes);
    }

    opts.excitation_wall_s = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - t_begin)
                                 .count();
    const int events_at_cutoff = events_printed;
    if (manual_trigger) {
      manual_trigger->Stop();
    }

    int drained_events = 0;
    bool drain_reached_quiet = false;
    if (lecroy_output) {
      try {
        lecroy_output->Disable();
      } catch (const std::exception& error) {
        throw sampic::tests::ProbeFatalError(
            "Unable to inhibit the Lecroy output at capture cutoff: " +
            std::string(error.what()));
      }
      std::cout
          << "Lecroy output disabled at capture cutoff; draining queued "
             "SAMPIC data.\n";

      ReadoutConfig drain_readout = cfg.readout;
      drain_readout.retry_sleep_us =
          std::max(100, drain_readout.retry_sleep_us);
      drain_readout.max_loops = std::max(
          1,
          static_cast<int>(
              std::ceil(
                  opts.drain_quiet_ms * 1000.0 /
                  static_cast<double>(drain_readout.retry_sleep_us))));
      const auto drain_deadline =
          std::chrono::steady_clock::now() +
          std::chrono::duration<double>(opts.drain_timeout_s);

      while (std::chrono::steady_clock::now() < drain_deadline) {
        int hits = 0;
        int frames = 0;
        int bytes = 0;
        event = EventStruct{};
        if (!session.read_event(
                drain_readout,
                event,
                hits,
                frames,
                bytes,
                false)) {
          drain_reached_quiet = true;
          break;
        }
        record_event(hits, frames, bytes);
        ++drained_events;
      }

      if (!drain_reached_quiet) {
        std::cerr
            << "Warning: drain timeout reached before the DAQ stream stayed "
               "quiet for "
            << opts.drain_quiet_ms << " ms\n";
      }
    }

    const double capture_wall_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - t_begin)
                                      .count();
    const unsigned long lost_transport_frames =
        vendor_transport_counters_available ? LPD_GetLostFrames() : 0;
    const unsigned long transport_bytes =
        vendor_transport_counters_available ? LPD_GetTotalByteCount() : 0;
    session.stop_run();
    std::cout << "Captured " << events_at_cutoff
              << " event(s) before cutoff and " << drained_events
              << " queued event(s) during drain (" << events_printed
              << " total).\n";
    std::cout << "Decoded readout: " << decoded_readout_frames
              << " frame(s), " << decoded_readout_bytes << " bytes, "
              << std::fixed << std::setprecision(3)
              << (capture_wall_s > 0.0
                      ? static_cast<double>(decoded_readout_bytes) /
                            capture_wall_s / 1.0e6
                      : 0.0)
              << " MB/s over " << capture_wall_s << " s\n"
              << std::defaultfloat;
    if (vendor_transport_counters_available) {
      std::cout << "Vendor transport diagnostics: lost_frames="
                << lost_transport_frames << ", bytes=" << transport_bytes
                << ", rate=" << std::fixed << std::setprecision(3)
                << (capture_wall_s > 0.0
                        ? static_cast<double>(transport_bytes) /
                              capture_wall_s / 1.0e6
                        : 0.0)
                << " MB/s over " << capture_wall_s << " s\n"
                << std::defaultfloat;
    }
    std::cout << "\n=== Current frontend same-vendor-event association ===\n";
    print_summary(events_printed, total_stats);

    Options correlation_options = opts;
    if (opts.auto_hit_offset) {
      const auto estimated_offset =
          estimate_hit_time_offset_ns(observed_hits, observed_triggers);
      if (estimated_offset) {
        correlation_options.hit_time_offset_ns = *estimated_offset;
        std::cout << "Offline hit-offset estimate: " << *estimated_offset
                  << " ns (requested/default "
                  << opts.requested_hit_time_offset_ns << " ns)\n";
      } else {
        std::cerr
            << "Warning: automatic hit-offset estimation found no hit/trigger "
               "pairs within 10 us; using "
            << opts.hit_time_offset_ns << " ns.\n";
      }
    }
    print_stream_correlation(
        correlation_options,
        events_printed,
        events_at_cutoff,
        vendor_transport_counters_available,
        lost_transport_frames,
        transport_bytes,
        decoded_readout_bytes,
        decoded_readout_frames,
        capture_wall_s,
        std::move(observed_hits),
        std::move(observed_triggers));
    return 0;
  } catch (const sampic::tests::ProbeFatalError& ex) {
    std::cerr << "external_trigger_probe fatal error: " << ex.what() << "\n";
    return 2;
  } catch (const std::exception& ex) {
    std::cerr << "external_trigger_probe error: " << ex.what() << "\n";
    return 1;
  }
}
