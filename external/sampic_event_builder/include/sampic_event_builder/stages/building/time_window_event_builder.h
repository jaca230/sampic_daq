#pragma once

#include "sampic_event_builder/stages/building/event_building_stage.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace sampic::event_builder {

struct TimeWindowEventBuilderConfig {
    std::uint64_t window_ns = 200;
    /// Hold hits until this much newer timestamp data has arrived, allowing
    /// independently delivered crate streams to contribute to one window.
    std::uint64_t allowed_lateness_ns = 0;
    std::size_t expected_crates = 1;
};

class TimeWindowEventBuilder final : public EventBuildingStage {
public:
    explicit TimeWindowEventBuilder(std::uint64_t window_ns);
    explicit TimeWindowEventBuilder(TimeWindowEventBuilderConfig config)
        : window_ns_(config.window_ns),
          allowed_lateness_ns_(config.allowed_lateness_ns) {
        expected_crates_ = config.expected_crates;
        validate();
    }
    std::vector<Event> build(std::vector<Hit> hits) override;
    std::vector<Event> flush() override;
    std::string_view type() const override { return "time-window"; }

private:
    std::uint64_t window_ns_;
    std::uint64_t allowed_lateness_ns_{0};
    std::size_t expected_crates_{1};
    std::unordered_map<std::uint16_t, std::uint64_t> crate_watermarks_;
    std::vector<Hit> pending_;

    void validate() const;
    std::vector<Event> consume_pending(std::uint64_t watermark);
};

} // namespace sampic::event_builder
