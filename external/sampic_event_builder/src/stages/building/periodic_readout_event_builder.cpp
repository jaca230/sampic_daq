#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"

#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sampic::event_builder {

PeriodicReadoutEventBuilder::PeriodicReadoutEventBuilder(
    PeriodicReadoutEventBuilderConfig config,
    Now now)
    : config_(config), now_(std::move(now)) {
    if (config_.readout_period_ns == 0 ||
        config_.readout_period_ns >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("periodic readout period is invalid");
    }
    if (!now_) {
        throw std::invalid_argument("periodic readout clock is missing");
    }
}

std::vector<Event> PeriodicReadoutEventBuilder::build(std::vector<Hit> hits) {
    pending_.insert(
        pending_.end(),
        std::make_move_iterator(hits.begin()),
        std::make_move_iterator(hits.end()));
    if (!origin_) {
        origin_ = now_();
    }
    return readout(false);
}

std::vector<Event> PeriodicReadoutEventBuilder::flush() {
    if (!origin_) {
        return {};
    }
    auto result = readout(true);
    pending_.clear();
    origin_.reset();
    next_tick_ = 0;
    return result;
}

std::vector<Event> PeriodicReadoutEventBuilder::readout(bool final) {
    const auto current = now_();
    if (current < *origin_) {
        throw std::runtime_error("periodic readout clock moved backwards");
    }
    const auto elapsed_signed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        current - *origin_).count();
    const auto elapsed = static_cast<std::uint64_t>(elapsed_signed);
    const auto completed = elapsed / config_.readout_period_ns;
    if (!final && completed <= next_tick_) {
        return {};
    }
    if (final && pending_.empty()) {
        return {};
    }
    if (!config_.emit_empty_readouts && pending_.empty()) {
        next_tick_ = completed;
        return {};
    }

    const auto start = next_tick_ * config_.readout_period_ns;
    const auto end = final ? elapsed : completed * config_.readout_period_ns;
    Event event{start, end, std::move(pending_)};
    pending_.clear();
    next_tick_ = completed;
    return {std::move(event)};
}

} // namespace sampic::event_builder
