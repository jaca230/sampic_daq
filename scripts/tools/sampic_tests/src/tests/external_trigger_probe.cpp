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

#include "external_trigger_probe/runtime_and_options.h"
#include "external_trigger_probe/sampic_session.h"
#include "external_trigger_probe/trigger_diagnostics.h"
#include "external_trigger_probe/acquisition_sequencer.h"
#include "external_trigger_probe/persistent_batching_scan.h"

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
    opts.sampling_frequency_requested_mhz =
        session.sampling_frequency_requested_mhz();
    opts.sampling_frequency_readback_mhz =
        session.sampling_frequency_readback_mhz();
    opts.sampling_frequency_external_clock =
        session.sampling_frequency_external_clock();
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
