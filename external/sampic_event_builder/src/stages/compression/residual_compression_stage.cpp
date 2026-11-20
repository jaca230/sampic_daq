#include "sampic_event_builder/stages/compression/residual_compression_stage.h"

#include <cstring>

namespace sampic::event_builder {
void ResidualCompressionStage::compress(PipelineEvent& event) const {
    for(auto& item:event.waveforms) {
        auto& p=item.product;
        if ((p.flags & ProcessingFlags::fit_rejected) != 0 &&
            !compress_rejected_fits_) {
            p.payload.resize(item.values.size() * sizeof(std::int16_t));
            for (std::size_t i = 0; i < item.values.size(); ++i) {
                const auto sample=static_cast<std::int16_t>(item.values[i]);
                std::memcpy(p.payload.data()+i*sizeof(sample),&sample,sizeof(sample));
            }
            p.payload_bit_count=p.payload.size()*8;
            continue;
        }
        auto block=codec_.encode(item.values);
        p.flags|=ProcessingFlags::compressed; p.codec_id=block.codec_id;
        p.payload_bit_count=block.bit_count; p.payload=std::move(block.bytes);
    }
}
} // namespace sampic::event_builder
