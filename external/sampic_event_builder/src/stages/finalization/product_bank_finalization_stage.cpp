#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"

namespace sampic::event_builder {
FinalizedEvent ProductBankFinalizationStage::finalize(const PipelineEvent& event) const {
    std::vector<ProcessedWaveform> products; products.reserve(event.waveforms.size());
    for(const auto& item:event.waveforms) products.push_back(item.product);
    return {event.source.start_ns,event.source.end_ns,ProductBankEncoder{}.encode(products)};
}
} // namespace sampic::event_builder
