#pragma once
#include "sampic_event_builder/stages/selection/selection_stage.h"
namespace sampic::event_builder {
// Deterministic testing cut: clears the payload of every Nth nonempty event.
class DummyEventSelector final : public SelectionStage {
public:
    static constexpr std::string_view kId="dummy";
    explicit DummyEventSelector(std::uint64_t every=2):every_(every) {
        if (!every) throw std::invalid_argument("dummy rejection interval must be positive");
    }
    std::int32_t default_priority() const override { return 200; }
    std::string_view id() const override { return kId; }
    std::string_view type() const override { return kId; }
    EventDecision select(Event& event) const override {
        if (++seen_%every_!=0) return EventDecision::keep;
        event.hits.clear();
        return EventDecision::reject_dummy;
    }
    void reset_counters() const noexcept override { SelectionStage::reset_counters(); seen_=0; }
private:
    std::uint64_t every_;
    mutable std::uint64_t seen_=0;
};
}
