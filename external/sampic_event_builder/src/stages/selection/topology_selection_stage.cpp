#include "sampic_event_builder/stages/selection/topology_selection_stage.h"

#include <algorithm>
#include <stdexcept>

namespace sampic::event_builder {

TopologyEventSelector::TopologyEventSelector(TopologySelectorConfig config) : config_(config)
{
    if (config_.plane_count < 2 || config_.channels_per_plane == 0)
        throw std::invalid_argument("ATAR topology dimensions must be positive");
}

EventDecision TopologyEventSelector::select(Event& event) const
{
    double integral = 0;
    std::vector<bool> occupied(config_.plane_count, false);
    for (const auto& hit : event.hits) {
        if (hit.sample_count()==0) continue;
        const auto baseline_count = std::min<std::size_t>(8, hit.sample_count());
        double baseline = 0;
        for (std::size_t index = 0; index < baseline_count; ++index) baseline += hit.corrected_sample(index);
        baseline /= static_cast<double>(baseline_count);
        for (std::size_t index=0; index<hit.sample_count(); ++index)
            integral += std::max(0.0,hit.corrected_sample(index)-baseline);
        const auto global_channel =
            static_cast<std::size_t>(hit.board) * 64 + hit.channel;
        const auto plane = global_channel / config_.channels_per_plane;
        if (plane < occupied.size()) occupied[plane] = true;
    }
    const auto occupied_planes = std::ranges::count(occupied, true);
    const bool traverses_atar = occupied.front() && occupied.back() &&
        occupied_planes == static_cast<std::ptrdiff_t>(config_.plane_count);
    const bool low_amplitude = !event.hits.empty() &&
        integral / static_cast<double>(event.hits.size()) <=
            config_.maximum_punch_through_integral_per_hit;
    if (traverses_atar && low_amplitude) {
        event.hits.clear();
        return EventDecision::reject_through_going;
    }
    return EventDecision::keep;
}

} // namespace sampic::event_builder
