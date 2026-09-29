#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "runtime_and_options.h"
#include "sampic_session.h"
#include "trigger_diagnostics.h"
#include "sampic_tests/batching_scan/batching_scan_config.h"
#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"
#include "sampic_tests/modes/double_pulse/config.h"

using sampic::double_pulse::DoublePulseConfig;
using sampic::double_pulse::ReadoutConfig;
class AcquisitionSequencer {
 public:
  AcquisitionSequencer(
      const DoublePulseConfig& hardware_config,
      SimpleSession& session,
      sampic::lecroy::LecroyClient& lecroy,
      sampic::lecroy::LecroyOutputGate& lecroy_output)
      : hardware_config_(hardware_config),
        session_(session),
        lecroy_(lecroy),
        lecroy_output_(lecroy_output) {}

  static void ConfigureSession(
      SimpleSession& session,
      const DoublePulseConfig& hardware_config,
      const sampic::batching_scan::BatchingScanConfig& scan_config,
      sampic::batching_scan::AcquisitionScheme acquisition_scheme) {
    const bool all_channels = scan_config.enabled_channels.empty();
    if (all_channels) {
      session.enable_all_channels();
    } else {
      session.enable_channels(
          hardware_config.scan.board_index, scan_config.enabled_channels);
    }
    if (acquisition_scheme ==
        sampic::batching_scan::AcquisitionScheme::L2ExternalGate) {
      session.enable_l2_external_gate(
          all_channels,
          hardware_config.scan.board_index,
          all_channels ? hardware_config.scan.channels
                       : scan_config.enabled_channels,
          scan_config.primitive_gate_clocks,
          scan_config.latency_gate_clocks,
          scan_config.external_gate_clocks);
    } else if (acquisition_scheme ==
               sampic::batching_scan::AcquisitionScheme::SelfTrigger) {
      session.enable_plain_self_trigger();
    } else {
      session.enable_plain_external_trigger();
    }
  }

// One hardware acquisition, intentionally ordered as explicit phases:
//   1. inhibit/program Lecroy
//   2. allocate acquisition buffers, then reset transport counters
//   3. start SAMPIC and prepare the receiver while Lecroy remains inhibited
//   4. enable Lecroy, then immediately acquire until cutoff
//   5. inhibit Lecroy, then drain queued SAMPIC data
//   6. stop SAMPIC
// Analysis/export follows StopRun and does not touch acquisition hardware.
// See SAMPIC_ACQUISITION_PROCEDURE.md for the discussion-facing overview.
  void Capture(Options& options) {
    const auto& hardware_config = hardware_config_;
    auto& session = session_;
    auto& lecroy = lecroy_;
    auto& lecroy_output = lecroy_output_;
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
  
    // Prepare every potentially expensive in-memory object before StartRun and
    // before the Lecroy outputs can emit triggers. EventStruct is about 7.3 MiB
    // with this vendor library, and the pipelined raw queue owns its full pool of
    // frame buffers.
    auto event = std::make_unique<EventStruct>();
    std::unique_ptr<EventStruct> pipelined_decoded_event;
    std::unique_ptr<RawVendorEventQueue> pipelined_raw_queue;
    if (options.pipelined_decode) {
      pipelined_decoded_event = std::make_unique<EventStruct>();
      pipelined_raw_queue =
          std::make_unique<RawVendorEventQueue>(options.raw_queue_capacity);
    }
    int events_printed = 0;
    std::vector<ObservedHit> observed_hits;
    std::vector<ObservedTrigger> observed_triggers;
    if (options.lecroy_rate_readback_hz) {
      const auto expected_triggers = static_cast<std::size_t>(std::ceil(
          *options.lecroy_rate_readback_hz * options.max_duration_s * 1.10));
      observed_triggers.reserve(expected_triggers);
      observed_hits.reserve(expected_triggers * 8);
    }
    options.collection_timings.clear();
    options.collection_timings.reserve(
        static_cast<std::size_t>(options.max_events));
  
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
      if (options.post_start_settle_s > 0.0) {
        std::cout << "Capture setup: SAMPIC run started; waiting "
                  << options.post_start_settle_s
                  << " s before enabling Lecroy output." << std::endl;
        std::this_thread::sleep_for(
            std::chrono::duration<double>(options.post_start_settle_s));
      }
      auto enable_generator = [&]() {
        std::cout << "Capture setup: enabling Lecroy output." << std::endl;
        try {
          lecroy_output.Enable();
        } catch (const std::exception& error) {
          throw sampic::tests::ProbeFatalError(
              "Unable to enable the Lecroy output for acquisition: " +
              std::string(error.what()));
        }
        std::cout << "Capture setup complete; acquisition active." << std::endl;
        return std::chrono::steady_clock::now();
      };
  
      std::chrono::steady_clock::time_point capture_start;
      std::uint64_t decoded_readout_bytes = 0;
      std::uint64_t decoded_readout_frames = 0;
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
          ObservedHit observed{
              event_index,
              hit_index,
              hit.FeBoardIndex,
              hit.SampicIndex,
              hit.Channel,
              hit.FirstCellTimeStamp +
                  hit.AdvancedParams.FirstTriggerPositionCell *
                      sample_period_ns,
              hit.FirstCellTimeStamp,
              hit.AdvancedParams.FirstTriggerPositionCell};
          if (observed_hits.size() < options.startup_waveform_hits) {
            const int sample_count =
                std::clamp(hit.DataSize, 0, MAX_NB_OF_SAMPLES);
            observed.corrected_samples.assign(
                hit.CorrectedDataSamples,
                hit.CorrectedDataSamples + sample_count);
            observed.raw_samples.assign(
                hit.OrderedRawDataSamples,
                hit.OrderedRawDataSamples + sample_count);
          }
          observed_hits.push_back(std::move(observed));
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
          if (options.use_l2_external_gate) {
            // Stop the external gate before stopping the analog pulses. Any
            // channel-A pulses emitted while the second command completes are
            // rejected by the disabled L2 gate instead of producing a tail of
            // external-trigger rows with no corresponding channel hits.
            lecroy_output.DisableInOrder({"B", "A"});
          } else {
            lecroy_output.Disable();
          }
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
        auto& raw_queue = *pipelined_raw_queue;
        auto decoder_info = std::make_unique<CrateInfoStruct>();
        auto decoder_params = std::make_unique<CrateParamStruct>();
        session.copy_decoder_context(*decoder_info, *decoder_params);
        std::exception_ptr decoder_error;
        std::atomic<bool> decoder_failed{false};
        std::mutex worker_mutex;
        std::condition_variable worker_cv;
        bool decoder_ready = false;
        bool receiver_ready = false;
        std::atomic<bool> cutoff_requested{false};
        std::atomic<bool> receiver_failed{false};
        std::atomic<int> received_events{0};
        std::exception_ptr receiver_error;
  
        std::thread decoder([&]() {
          try {
            {
              std::lock_guard<std::mutex> lock(worker_mutex);
              decoder_ready = true;
            }
            worker_cv.notify_all();
            while (auto* raw = raw_queue.pop()) {
              int hits = 0;
              if (!SimpleSession::decode_raw_event(
                      *decoder_info,
                      *decoder_params,
                      *raw,
                      *pipelined_decoded_event,
                      hits)) {
                throw std::runtime_error("SAMPIC256CH_DecodeEvent failed");
              }
              const auto processing_start = std::chrono::steady_clock::now();
              record_event(
                  *pipelined_decoded_event,
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
            worker_cv.notify_all();
          }
        });
  
        std::thread receiver;
        try {
          auto receive_one = [&](const ReadoutConfig& readout,
                               bool during_drain,
                               bool report_timeout) {
          RawVendorEvent* raw = raw_queue.acquire();
          if (!raw) return false;
          {
            std::lock_guard<std::mutex> lock(worker_mutex);
            if (!receiver_ready) receiver_ready = true;
          }
          worker_cv.notify_all();
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
          raw->event_index = received_events.fetch_add(1) + 1;
          raw->during_drain = during_drain || cutoff_requested.load();
          raw_queue.publish(raw);
          worker_cv.notify_all();
          return true;
          };
  
          receiver = std::thread([&]() {
            try {
              // This is the only thread that calls the vendor receive API. It
              // enters that path before the control thread enables the Lecroy.
              while (!cutoff_requested && !decoder_failed &&
                     received_events < options.max_events) {
                receive_one(hardware_config.readout, false, true);
              }
              worker_cv.notify_all();
  
              // If the event limit ended active reception, wait until the
              // control thread has inhibited the Lecroy before draining.
              if (!cutoff_requested && !decoder_failed) {
                std::unique_lock<std::mutex> lock(worker_mutex);
                worker_cv.wait(lock, [&]() {
                  return cutoff_requested.load() || decoder_failed.load();
                });
              }
  
              const auto drain_deadline =
                  std::chrono::steady_clock::now() +
                  std::chrono::duration<double>(options.drain_timeout_s);
              while (std::chrono::steady_clock::now() < drain_deadline &&
                     !decoder_failed &&
                     receive_one(drain_readout, true, false)) {
              }
            } catch (...) {
              receiver_error = std::current_exception();
              receiver_failed = true;
            }
            raw_queue.close();
            worker_cv.notify_all();
          });
  
          // Both workers now exist, and the receiver is about to enter (or has
          // entered) its first vendor read while the Lecroy is still inhibited.
          {
            std::unique_lock<std::mutex> lock(worker_mutex);
            worker_cv.wait(lock, [&]() {
              return (decoder_ready && receiver_ready) ||
                  receiver_failed.load() || decoder_failed.load();
            });
          }
          if (receiver_failed || decoder_failed) {
            throw std::runtime_error(
                "Pipelined worker failed before Lecroy enable");
          }
          capture_start = enable_generator();
  
          const auto capture_deadline =
              capture_start + std::chrono::duration<double>(options.max_duration_s);
          {
            std::unique_lock<std::mutex> lock(worker_mutex);
            worker_cv.wait_until(lock, capture_deadline, [&]() {
              return received_events.load() >= options.max_events ||
                  receiver_failed.load() || decoder_failed.load();
            });
          }
          options.excitation_wall_s = std::chrono::duration<double>(
                                          std::chrono::steady_clock::now() -
                                          capture_start)
                                          .count();
          disable_generator_at_cutoff();
          events_at_cutoff = received_events.load();
          cutoff_requested = true;
          worker_cv.notify_all();
          receiver.join();
          decoder.join();
          drained_events = received_events.load() - events_at_cutoff;
          options.raw_queue_high_water_mark = raw_queue.high_water_mark();
          options.raw_queue_producer_wait_us = raw_queue.producer_wait_us();
          if (receiver_error) std::rethrow_exception(receiver_error);
          if (decoder_error) std::rethrow_exception(decoder_error);
          events_printed = received_events.load();
        } catch (...) {
          cutoff_requested = true;
          worker_cv.notify_all();
          raw_queue.close();
          if (receiver.joinable()) receiver.join();
          if (decoder.joinable()) decoder.join();
          throw;
        }
      } else {
        capture_start = enable_generator();
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
  
 private:
  const DoublePulseConfig& hardware_config_;
  SimpleSession& session_;
  sampic::lecroy::LecroyClient& lecroy_;
  sampic::lecroy::LecroyOutputGate& lecroy_output_;
};
