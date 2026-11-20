#include "sampic_event_builder/stages/building/time_window_event_builder.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace sampic::event_builder {

TimeWindowEventBuilder::TimeWindowEventBuilder(std::uint64_t window_ns) : window_ns_(window_ns)
{
    validate();
}

void TimeWindowEventBuilder::validate() const {
    if (window_ns_ == 0) {
        throw std::invalid_argument("event window must be positive");
    }
    if (expected_crates_ == 0) {
        throw std::invalid_argument("expected crate count must be positive");
    }
}

std::vector<Event> TimeWindowEventBuilder::build(std::vector<Hit> hits)
{
    std::ranges::sort(hits, {}, &Hit::timestamp_ns);
    if (allowed_lateness_ns_ == 0) {
        std::vector<Event> events;
        for (auto& hit : hits) {
            if (events.empty() ||
                hit.timestamp_ns - events.back().start_ns >= window_ns_) {
                events.push_back(Event{hit.timestamp_ns, hit.timestamp_ns, {}});
            }
            events.back().end_ns = hit.timestamp_ns;
            events.back().hits.push_back(std::move(hit));
        }
        return events;
    }

    const auto previous_size = pending_.size();
    for (const auto& hit : hits) {
        auto& crate_timestamp = crate_watermarks_[hit.crate];
        crate_timestamp = std::max(crate_timestamp, hit.timestamp_ns);
    }
    pending_.insert(
        pending_.end(),
        std::make_move_iterator(hits.begin()),
        std::make_move_iterator(hits.end()));
    std::inplace_merge(
        pending_.begin(), pending_.begin() + previous_size, pending_.end(),
        [](const Hit& left, const Hit& right) {
            return left.timestamp_ns < right.timestamp_ns;
        });

    if (crate_watermarks_.size() < expected_crates_) {
        return {};
    }
    const auto slowest_crate = std::min_element(
        crate_watermarks_.begin(), crate_watermarks_.end(),
        [](const auto& left, const auto& right) {
            return left.second < right.second;
        })->second;
    const std::uint64_t watermark = slowest_crate > allowed_lateness_ns_
        ? slowest_crate - allowed_lateness_ns_
        : 0;
    return consume_pending(watermark);
}

std::vector<Event> TimeWindowEventBuilder::flush()
{
    if (pending_.empty()) {
        crate_watermarks_.clear();
        return {};
    }
    const auto final_timestamp = pending_.back().timestamp_ns;
    auto events = consume_pending(
        final_timestamp > std::numeric_limits<std::uint64_t>::max() - window_ns_
            ? std::numeric_limits<std::uint64_t>::max()
            : final_timestamp + window_ns_);
    pending_.clear();
    crate_watermarks_.clear();
    return events;
}

std::vector<Event> TimeWindowEventBuilder::consume_pending(
    std::uint64_t watermark)
{
    std::vector<Event> events;
    std::size_t consumed = 0;
    while (consumed < pending_.size()) {
        const std::size_t group_begin = consumed;
        const std::uint64_t group_start = pending_[group_begin].timestamp_ns;
        while (consumed < pending_.size() &&
               pending_[consumed].timestamp_ns - group_start < window_ns_) {
            ++consumed;
        }

        if (group_start > watermark ||
            watermark - group_start < window_ns_) {
            consumed = group_begin;
            break;
        }
        Event event{group_start, pending_[consumed - 1].timestamp_ns, {}};
        event.hits.insert(
            event.hits.end(),
            std::make_move_iterator(pending_.begin() + group_begin),
            std::make_move_iterator(pending_.begin() + consumed));
        events.push_back(std::move(event));
    }
    if (consumed != 0) {
        pending_.erase(pending_.begin(), pending_.begin() + consumed);
    }
    return events;
}

} // namespace sampic::event_builder
