#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
namespace sampic::event_builder {
FinalizedEvent ProductBankFinalizationStage::finalize(const Event& event) const {
    return {event.start_ns, event.end_ns, ProductBankEncoder{}.encode(event)};
}
}
