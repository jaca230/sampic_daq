#pragma once
#include "sampic_event_builder/stages/selection/selection_stage.h"
#include <memory>
#include <algorithm>
namespace sampic::event_builder {
// Lower priority first, then ID. Each cut sees surviving hits; retain the envelope.
class CutChain final : public SelectionStage {
public:
    std::string_view id() const override { return "cut_chain"; }
    std::string_view type() const override { return "cut_chain"; }
    void add(std::unique_ptr<SelectionStage> cut) {
        if (!cut || !selection::valid_id(std::string(cut->id()))) throw std::invalid_argument("invalid cut registration");
        for (const auto& prior:cuts_) if (prior->id()==cut->id()) throw std::invalid_argument("duplicate cut ID");
        cuts_.push_back(std::move(cut));
        std::ranges::sort(cuts_,[](const auto& a,const auto& b) {
            return a->priority()!=b->priority() ? a->priority()<b->priority() : a->id()<b->id();
        });
    }
    EventDecision select(Event& event) const override {
        auto result=EventDecision::keep;
        for (const auto& cut:cuts_) {
            if (event.hits.empty()) break;
            const auto decision=cut->apply(event);
            if (decision!=EventDecision::keep) result=decision;
        }
        return result;
    }
    std::vector<selection::Summary> summaries(bool take) const override {
        std::vector<selection::Summary> result;
        for (const auto& cut:cuts_) {
            auto rows=cut->summaries(take);
            result.insert(result.end(),std::make_move_iterator(rows.begin()),std::make_move_iterator(rows.end()));
        }
        return result;
    }
    void reset_counters() const noexcept override {
        SelectionStage::reset_counters();
        for (const auto& cut:cuts_) cut->reset_counters();
    }
private:
    std::vector<std::unique_ptr<SelectionStage>> cuts_;
};
}
