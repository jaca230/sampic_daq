#pragma once
#include "sampic_event_builder/model/event.h"
#include "sampic_event_builder/pipeline/stage.h"
#include "sampic_event_builder/model/selection_summary.h"
#include <optional>
#include <stdexcept>
namespace sampic::event_builder {
enum class EventDecision { keep, reject_through_going, reject_dummy };
class SelectionStage : public Stage {
public:
    virtual std::string_view id() const = 0;
    virtual EventDecision select(Event& event) const = 0;
    virtual std::int32_t default_priority() const { return 0; }
    std::int32_t priority() const { return priority_.value_or(default_priority()); }
    void set_priority(std::int32_t priority) { priority_=priority; }
    void set_enabled(bool enabled) { enabled_=enabled; }
    EventDecision apply(Event& event) const {
        if (!enabled_ || event.hits.empty()) return EventDecision::keep;
        const auto before=event.hits.size();
        const auto start=event.start_ns, end=event.end_ns;
        const auto decision=select(event);
        if (event.start_ns!=start || event.end_ns!=end)
            throw std::logic_error("selection cannot change event bounds");
        if (event.hits.size()>before) throw std::logic_error("selection cannot add hits");
        const auto removed=before-event.hits.size();
        for (auto* counts:{&pending_,&total_}) {
            ++counts->evaluated_events; counts->evaluated_hits+=before;
            // Legacy wire field rejected_events counts payloads modified by this cut.
            if (removed) ++counts->rejected_events;
            counts->rejected_hits+=removed;
        }
        return decision;
    }
    virtual std::vector<selection::Summary> summaries(bool take) const {
        auto result=take ? pending_ : total_;
        result.id=id(); result.enabled=enabled_; result.priority=priority();
        if (take) pending_={};
        return {std::move(result)};
    }
    virtual void reset_counters() const noexcept { pending_={}; total_={}; }
private:
    bool enabled_=true;
    std::optional<std::int32_t> priority_;
    mutable selection::Summary pending_,total_;
};
}
