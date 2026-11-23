#pragma once
#include "sampic_event_builder/stages/input/input_stage.h"
#include <utility>

namespace sampic::event_builder::testing {
class VectorInputStage final : public InputStage {
public:
    explicit VectorInputStage(std::vector<Hit> hits) : hits_(std::move(hits)) {}
    std::vector<Hit> read_available() override { return std::exchange(hits_,{}); }
    std::string_view type() const override { return "test-vector"; }
private: std::vector<Hit> hits_;
};
} // namespace sampic::event_builder::testing
