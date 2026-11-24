#include "sampic_event_builder/diagnostics/timing_accumulator.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace sampic::event_builder::timing {
static_assert(std::endian::native == std::endian::little);

namespace {
void add_checked(std::uint64_t& sum, std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max()-sum)
        throw std::overflow_error("timing summary overflow");
    sum += value;
}
}
void Accumulator::add(std::uint16_t crate, const Acquisition& a) {
    auto& state = states_[crate];
    if (state.last_id && (*state.last_id == UINT64_MAX || a.acquisition_id != *state.last_id+1))
        throw std::runtime_error("missing, duplicate or out-of-order SA acquisition ID");
    auto next = state.summary;
    if (!next.read_count) next.first_acquisition_id = a.acquisition_id;
    add_checked(next.read_count, 1); add_checked(next.hit_count, a.hit_count);
    next.last_acquisition_id = a.acquisition_id;
    add_checked(next.sum.prepare_us, a.duration.prepare_us);
    add_checked(next.sum.read_us, a.duration.read_us);
    add_checked(next.sum.decode_us, a.duration.decode_us);
    add_checked(next.sum.total_us, a.duration.total_us);
    next.maximum.prepare_us = std::max(next.maximum.prepare_us, a.duration.prepare_us);
    next.maximum.read_us = std::max(next.maximum.read_us, a.duration.read_us);
    next.maximum.decode_us = std::max(next.maximum.decode_us, a.duration.decode_us);
    next.maximum.total_us = std::max(next.maximum.total_us, a.duration.total_us);
    add_checked(next.retries_sum, a.retries);
    next.retries_max = std::max(next.retries_max, a.retries);
    state.summary = next; state.last_id = a.acquisition_id; ++reads_;
}
std::map<std::uint16_t, Summary> Accumulator::take(std::uint64_t cycle_id) {
    std::map<std::uint16_t, Summary> result;
    for (auto& [crate, state] : states_) {
        if (state.summary.read_count) {
            state.summary.cycle_id = cycle_id;
            result.emplace(crate, state.summary);
            state.summary = {};
        }
    }
    return result;
}
}
