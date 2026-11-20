#pragma once
#include "sampic_event_builder/stages/compression/compression_stage.h"

namespace sampic::event_builder {
class UncompressedStage final : public CompressionStage {
public:
    void compress(PipelineEvent& event) const override;
    std::string_view type() const override { return "none"; }
};
} // namespace sampic::event_builder
