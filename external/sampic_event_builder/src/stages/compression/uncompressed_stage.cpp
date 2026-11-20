#include "sampic_event_builder/stages/compression/uncompressed_stage.h"
#include <cstring>

namespace sampic::event_builder {
void UncompressedStage::compress(PipelineEvent& event) const {
    for(auto& item:event.waveforms) {
        auto& p=item.product;
        if((p.flags&ProcessingFlags::fitted)!=0) {
            p.payload.resize(item.values.size()*sizeof(std::int32_t));
            std::memcpy(p.payload.data(),item.values.data(),p.payload.size());
        } else {
            p.payload.resize(item.values.size()*sizeof(std::int16_t));
            for(std::size_t i=0;i<item.values.size();++i) {
                const auto sample=static_cast<std::int16_t>(item.values[i]);
                std::memcpy(p.payload.data()+i*sizeof(sample),&sample,sizeof(sample));
            }
        }
        p.payload_bit_count=p.payload.size()*8;
    }
}
} // namespace sampic::event_builder
