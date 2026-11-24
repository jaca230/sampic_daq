#pragma once

#include "sampic_event_builder/stages/selection/selection_stage.h"

#include <cstddef>
#include <cstdint>

namespace sampic::event_builder {

struct TopologySelectorConfig {
    std::size_t plane_count = 8;
    std::size_t channels_per_plane = 32;
    std::int64_t maximum_punch_through_integral_per_hit = 3'000;
};

class TopologyEventSelector final : public SelectionStage {
public:
    static constexpr std::string_view kId="topology";
    std::int32_t default_priority() const override { return 100; }
    std::string_view id() const override { return kId; }
    explicit TopologyEventSelector(TopologySelectorConfig config);
    EventDecision select(Event& event) const override;
    std::string_view type() const override { return "topology"; }

private:
    TopologySelectorConfig config_;
};

} // namespace sampic::event_builder
