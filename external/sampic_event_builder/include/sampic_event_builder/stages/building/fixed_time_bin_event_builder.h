#pragma once

#include "sampic_event_builder/stages/building/event_building_stage.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

namespace sampic::event_builder {

struct FixedTimeBinEventBuilderConfig {
    std::uint64_t bin_width_ns = 10'000;
    std::uint64_t bin_origin_ns = 0;
    std::uint64_t allowed_lateness_ns = 0;
    std::size_t expected_crates = 1;
    bool emit_empty_bins = true;
    std::size_t maximum_bins_per_build = 256;
};

/// Groups every input stream against the same hardware-time bin boundaries.
/// A bin is released only after all expected crate streams have advanced past
/// its end, less the configured out-of-order allowance.
class FixedTimeBinEventBuilder final : public EventBuildingStage {
public:
    explicit FixedTimeBinEventBuilder(FixedTimeBinEventBuilderConfig config);

    std::vector<Event> build(std::vector<Hit> hits) override;
    std::vector<Event> flush() override;
    std::string_view type() const override { return "fixed_time_bins"; }

private:
    using BinIndex = std::uint64_t;

    FixedTimeBinEventBuilderConfig config_;
    std::unordered_map<std::uint16_t, std::uint64_t> crate_watermarks_;
    std::map<BinIndex, std::vector<Hit>> bins_;
    std::optional<BinIndex> next_bin_;

    [[nodiscard]] BinIndex index_for(std::uint64_t timestamp_ns) const;
    [[nodiscard]] std::uint64_t start_for(BinIndex index) const;
    std::vector<Event> consume_through(BinIndex ready_through,
                                       std::size_t maximum_bins);
};

} // namespace sampic::event_builder
