#pragma once

#include "sampic_event_builder/stages/building/event_building_stage.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

namespace sampic::event_builder {

struct PeriodicReadoutEventBuilderConfig {
    std::uint64_t readout_period_ns = 10'000;
};

/// Mirrors the frontend's software-periodic readout, but drains hits already
/// merged from every subscribed SAMPIC stream.
class PeriodicReadoutEventBuilder final : public EventBuildingStage {
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;

    explicit PeriodicReadoutEventBuilder(
        PeriodicReadoutEventBuilderConfig config,
        Now now = Clock::now);

    std::vector<Event> build(std::vector<Hit> hits) override;
    std::vector<Event> flush() override;
    std::vector<Event> finish(std::vector<Hit> hits) override;
    std::optional<Clock::time_point> readout_deadline() const override {
        if (!origin_) return {};
        return *origin_ + std::chrono::nanoseconds((next_tick_ + 1) * config_.readout_period_ns);
    }
    std::string_view type() const override { return "periodic_readout"; }

private:
    PeriodicReadoutEventBuilderConfig config_;
    Now now_;
    std::optional<Clock::time_point> origin_;
    std::uint64_t next_tick_{0};
    std::vector<Hit> pending_;

    std::vector<Event> readout(bool final);
};

} // namespace sampic::event_builder
