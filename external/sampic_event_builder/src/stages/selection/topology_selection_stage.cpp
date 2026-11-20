#include "sampic_event_builder/stages/selection/topology_selection_stage.h"

#include <algorithm>
#include <stdexcept>

namespace sampic::event_builder {

TopologyEventSelector::TopologyEventSelector(TopologySelectorConfig config) : config_(config)
{
    if (config_.plane_count < 2 || config_.channels_per_plane == 0)
        throw std::invalid_argument("ATAR topology dimensions must be positive");
}

EventDecision TopologyEventSelector::select(const Event& event) const
{
    std::int64_t integral = 0;
    std::vector<bool> occupied(config_.plane_count, false);
    for (const auto& hit : event.hits) {
        if (hit.waveform.empty()) continue;
        const auto baseline_count = std::min<std::size_t>(8, hit.waveform.size());
        std::int64_t baseline = 0;
        for (std::size_t index = 0; index < baseline_count; ++index) baseline += hit.waveform[index];
        baseline /= static_cast<std::int64_t>(baseline_count);
        for (const auto sample : hit.waveform) integral += std::max<std::int64_t>(0, sample - baseline);
        const auto global_channel =
            static_cast<std::size_t>(hit.board) * 64 + hit.channel;
        const auto plane = global_channel / config_.channels_per_plane;
        if (plane < occupied.size()) occupied[plane] = true;
    }
    const auto occupied_planes = std::ranges::count(occupied, true);
    const bool traverses_atar = occupied.front() && occupied.back() &&
        occupied_planes == static_cast<std::ptrdiff_t>(config_.plane_count);
    const bool low_amplitude = !event.hits.empty() &&
        integral / static_cast<std::int64_t>(event.hits.size()) <=
            config_.maximum_punch_through_integral_per_hit;
    return traverses_atar && low_amplitude
        ? EventDecision::reject_through_going
        : EventDecision::keep;
}

} // namespace sampic::event_builder
