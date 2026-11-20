#include "sampic_event_builder/stages/fitting/passthrough_fitting_stage.h"

namespace sampic::event_builder {
void PassthroughFittingStage::fit(PipelineEvent& event) const {
    event.waveforms.clear(); event.waveforms.reserve(event.source.hits.size());
    for(const auto& hit:event.source.hits) {
        PipelineWaveform item;
        item.product.crate=hit.crate; item.product.board=hit.board; item.product.channel=hit.channel;
        item.product.timestamp_ns=hit.timestamp_ns; item.product.sample_count=hit.waveform.size();
        item.values.assign(hit.waveform.begin(),hit.waveform.end());
        event.waveforms.push_back(std::move(item));
    }
}
} // namespace sampic::event_builder
