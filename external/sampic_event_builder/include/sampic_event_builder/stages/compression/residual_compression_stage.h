#pragma once
#include "sampic_event_builder/stages/compression/compression_stage.h"
#include "residual_compression/residual_codec.h"

namespace sampic::event_builder {
class ResidualCompressionStage final : public CompressionStage {
public:
    explicit ResidualCompressionStage(
        const waveform::compression::ResidualCodec& codec,
        bool compress_rejected_fits = false)
        : codec_(codec), compress_rejected_fits_(compress_rejected_fits) {}
    void compress(PipelineEvent& event) const override;
    std::string_view type() const override { return "residual-codec"; }
private:
    const waveform::compression::ResidualCodec& codec_;
    bool compress_rejected_fits_;
};
} // namespace sampic::event_builder
