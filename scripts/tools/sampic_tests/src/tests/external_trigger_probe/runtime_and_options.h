#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <execinfo.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

extern "C" {
#include <lpDevC.h>
#include <SAMPIC_256Ch_Type.h>
}

inline volatile std::sig_atomic_t g_stop_after_current_point = 0;
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
  double post_start_settle_s = 0.0;
  std::size_t startup_waveform_hits = 0;
  double excitation_wall_s = 0.0;
  bool use_self_trigger_channels = false;
  bool use_l2_external_gate = false;
  std::string acquisition_scheme = "l2_external_gate";
  bool skip_lecroy = false;
  bool manage_lecroy_output = false;
  std::optional<double> lecroy_rate_hz;
  std::optional<double> lecroy_rate_readback_hz;
  int sampling_frequency_requested_mhz = 0;
  int sampling_frequency_readback_mhz = 0;
  bool sampling_frequency_external_clock = false;
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
