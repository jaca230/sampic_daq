#include "sampic_event_builder/stages/building/fixed_time_bin_event_builder.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sampic::event_builder {

FixedTimeBinEventBuilder::FixedTimeBinEventBuilder(
    FixedTimeBinEventBuilderConfig config)
    : config_(config) {
    if (config_.bin_width_ns == 0) {
        throw std::invalid_argument("fixed time-bin width must be positive");
    }
    if (config_.expected_crates == 0) {
        throw std::invalid_argument("expected crate count must be positive");
    }
    if (config_.maximum_bins_per_build == 0) {
        throw std::invalid_argument("maximum bins per build must be positive");
    }
}

FixedTimeBinEventBuilder::BinIndex
FixedTimeBinEventBuilder::index_for(std::uint64_t timestamp_ns) const {
    if (timestamp_ns < config_.bin_origin_ns) {
        throw std::runtime_error("hit timestamp precedes fixed time-bin origin");
    }
    return (timestamp_ns - config_.bin_origin_ns) / config_.bin_width_ns;
}

std::uint64_t FixedTimeBinEventBuilder::start_for(BinIndex index) const {
    if (index > (std::numeric_limits<std::uint64_t>::max() -
                 config_.bin_origin_ns) / config_.bin_width_ns) {
        throw std::overflow_error("fixed time-bin boundary overflow");
    }
    return config_.bin_origin_ns + index * config_.bin_width_ns;
}

std::vector<Event> FixedTimeBinEventBuilder::build(std::vector<Hit> hits) {
    for (auto& hit : hits) {
        auto& watermark = crate_watermarks_[hit.crate];
        watermark = std::max(watermark, hit.timestamp_ns);
        const auto index = index_for(hit.timestamp_ns);
        if (next_bin_ && index < *next_bin_) {
            throw std::runtime_error(
                "late hit targets an already emitted fixed time bin");
        }
        if (!next_bin_ || index < *next_bin_) {
            next_bin_ = index;
        }
        bins_[index].push_back(std::move(hit));
    }

    if (!next_bin_ || crate_watermarks_.size() < config_.expected_crates) {
        return {};
    }
    const auto slowest = std::min_element(
        crate_watermarks_.begin(), crate_watermarks_.end(),
        [](const auto& left, const auto& right) {
            return left.second < right.second;
        })->second;
    const auto safe_timestamp = slowest > config_.allowed_lateness_ns
        ? slowest - config_.allowed_lateness_ns
        : 0;
    if (safe_timestamp < config_.bin_origin_ns + config_.bin_width_ns) {
        return {};
    }
    const auto ready_through =
        index_for(safe_timestamp - config_.bin_width_ns);
    return consume_through(ready_through, config_.maximum_bins_per_build);
}

std::vector<Event> FixedTimeBinEventBuilder::flush() {
    if (!next_bin_) {
        crate_watermarks_.clear();
        return {};
    }
    const auto final_bin = bins_.empty() ? *next_bin_ : bins_.rbegin()->first;
    auto events = consume_through(final_bin, std::numeric_limits<std::size_t>::max());
    bins_.clear();
    crate_watermarks_.clear();
    next_bin_.reset();
    return events;
}

std::vector<Event> FixedTimeBinEventBuilder::consume_through(
    BinIndex ready_through,
    std::size_t maximum_bins) {
    std::vector<Event> result;
    while (next_bin_ && *next_bin_ <= ready_through &&
           result.size() < maximum_bins) {
        if (!config_.emit_empty_bins) {
            const auto found = bins_.lower_bound(*next_bin_);
            if (found == bins_.end() || found->first > ready_through) {
                next_bin_ = ready_through == std::numeric_limits<BinIndex>::max()
                    ? std::nullopt
                    : std::optional<BinIndex>{ready_through + 1};
                break;
            }
            next_bin_ = found->first;
        }

        const auto index = *next_bin_;
        const auto start = start_for(index);
        if (start > std::numeric_limits<std::uint64_t>::max() -
                        config_.bin_width_ns) {
            throw std::overflow_error("fixed time-bin end overflow");
        }
        Event event{start, start + config_.bin_width_ns, {}};
        if (auto found = bins_.find(index); found != bins_.end()) {
            event.hits = std::move(found->second);
            bins_.erase(found);
        }
        result.push_back(std::move(event));
        next_bin_ = index == std::numeric_limits<BinIndex>::max()
            ? std::nullopt
            : std::optional<BinIndex>{index + 1};
    }
    return result;
}

} // namespace sampic::event_builder
