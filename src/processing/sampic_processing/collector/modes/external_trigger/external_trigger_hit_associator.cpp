#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_hit_associator.h"

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

ExternalTriggerHitAssociator::ExternalTriggerHitAssociator(
    double hit_time_offset_ns,
    double pre_window_ns,
    double post_window_ns,
    double sampling_frequency_mhz)
    : hit_time_offset_ns_(hit_time_offset_ns),
      pre_window_ns_(pre_window_ns),
      post_window_ns_(post_window_ns),
      sample_period_ns_(1000.0 / sampling_frequency_mhz) {
    if (pre_window_ns < 0.0 || post_window_ns < 0.0 ||
        sampling_frequency_mhz <= 0.0) {
        throw std::invalid_argument(
            "association windows must be non-negative and sampling frequency positive");
    }
}

ExternalTriggerAssociationResult ExternalTriggerHitAssociator::associate(
    const EventStruct& event) const {
    ExternalTriggerAssociationResult result;

    const int trigger_count = std::max(0, event.TriggerData.NbOfTriggers);
    const int hit_count = std::max(0, event.NbOfHitsInEvent);
    result.stats.raw_hits = static_cast<std::size_t>(hit_count);
    result.stats.trigger_records = static_cast<std::size_t>(trigger_count);
    result.hits_by_trigger.resize(result.stats.trigger_records);
    result.ambiguous_hits_by_trigger.resize(result.stats.trigger_records, 0);
    result.hit_decisions.reserve(result.stats.raw_hits);

    for (int hit_index = 0; hit_index < hit_count; ++hit_index) {
        const HitStruct& hit = event.Hit[hit_index];
        ExternalTriggerHitDecision decision;
        decision.hit_index = static_cast<std::size_t>(hit_index);
        decision.hit_timestamp_ns =
            hit.FirstCellTimeStamp +
            hit.AdvancedParams.FirstTriggerPositionCell * sample_period_ns_;

        if (trigger_count == 0) {
            decision.disposition = ExternalTriggerHitDisposition::NoTriggerRecords;
            ++result.stats.hits_without_trigger_records;
            result.hit_decisions.push_back(decision);
            continue;
        }

        std::size_t nearest_index = 0;
        double nearest_distance = std::numeric_limits<double>::infinity();
        std::size_t triggers_inside_window = 0;

        for (int trigger_index = 0; trigger_index < trigger_count; ++trigger_index) {
            const double reference =
                event.TriggerData.TriggerTimeStamp[trigger_index] + hit_time_offset_ns_;
            const double delta = decision.hit_timestamp_ns - reference;
            const double distance = std::abs(delta);
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest_index = static_cast<std::size_t>(trigger_index);
            }
            if (delta >= -pre_window_ns_ && delta <= post_window_ns_) {
                ++triggers_inside_window;
            }
        }

        decision.nearest_trigger_index = nearest_index;
        decision.triggers_inside_window = triggers_inside_window;
        decision.nearest_trigger_timestamp_ns =
            event.TriggerData.TriggerTimeStamp[nearest_index];
        decision.nearest_reference_timestamp_ns =
            decision.nearest_trigger_timestamp_ns + hit_time_offset_ns_;
        decision.hit_minus_reference_ns =
            decision.hit_timestamp_ns - decision.nearest_reference_timestamp_ns;

        if (decision.hit_minus_reference_ns < -pre_window_ns_ ||
            decision.hit_minus_reference_ns > post_window_ns_) {
            decision.disposition =
                ExternalTriggerHitDisposition::OutsideAssociationWindow;
            ++result.stats.hits_outside_window;
            result.hit_decisions.push_back(decision);
            continue;
        }

        decision.disposition = ExternalTriggerHitDisposition::Assigned;
        decision.assigned_trigger_index = nearest_index;
        result.hits_by_trigger[nearest_index].push_back(&hit);
        ++result.stats.assigned_hits;
        if (triggers_inside_window > 1) {
            ++result.ambiguous_hits_by_trigger[nearest_index];
            ++result.stats.hits_inside_multiple_windows;
        }
        result.hit_decisions.push_back(decision);
    }

    result.stats.triggers_with_hits = static_cast<std::size_t>(std::count_if(
        result.hits_by_trigger.begin(),
        result.hits_by_trigger.end(),
        [](const auto& hits) { return !hits.empty(); }));
    result.stats.triggers_without_hits =
        result.stats.trigger_records - result.stats.triggers_with_hits;
    return result;
}
