#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "runtime_and_options.h"
#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_hit_associator.h"
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
  std::vector<float> corrected_samples;
  std::vector<unsigned short> raw_samples;
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

  if (options.startup_waveform_hits > 0) {
    auto stream = open_output("startup_waveforms.csv");
    stream << "capture_hit_index,event_index,hit_index,feb,sampic,channel,"
              "hit_timestamp_ns,trigger_position_cell,sample_index,"
              "corrected_sample,raw_sample\n";
    std::size_t capture_index = 0;
    for (const auto& hit : hits) {
      if (hit.corrected_samples.empty()) continue;
      const auto samples = std::min(
          hit.corrected_samples.size(), hit.raw_samples.size());
      for (std::size_t sample = 0; sample < samples; ++sample) {
        stream << capture_index << "," << hit.event_index << ","
               << hit.hit_index << "," << hit.feb << "," << hit.sampic
               << "," << hit.channel << "," << hit.timestamp_ns << ","
               << hit.trigger_position_cell << "," << sample << ","
               << hit.corrected_samples[sample] << ","
               << hit.raw_samples[sample] << "\n";
      }
      ++capture_index;
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
      {"dedicated_receiver_thread", options.pipelined_decode},
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
           {"post_start_settle_s", options.post_start_settle_s},
           {"startup_waveform_hits", options.startup_waveform_hits},
           {"excitation_wall_s", options.excitation_wall_s},
           {"acquisition_scheme", options.acquisition_scheme},
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
           {"sampling_frequency_requested_mhz",
            options.sampling_frequency_requested_mhz},
           {"sampling_frequency_readback_mhz",
            options.sampling_frequency_readback_mhz},
           {"sampling_frequency_external_clock",
            options.sampling_frequency_external_clock},
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
