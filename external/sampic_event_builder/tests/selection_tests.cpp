#include "sampic_event_builder/banks/selection_summary_bank.h"
#include "sampic_event_builder/stages/selection/cut_chain.h"
#include "sampic_event_builder/stages/selection/dummy_selection_stage.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/testing/vector_input_stage.h"
#include <cassert>
#include <cstring>
using namespace sampic::event_builder;
template<class Cut> EventDecision apply_copy(Cut& cut, Event event) { return cut.apply(event); }
class RemoveFirstHit final : public SelectionStage {
public:
    std::string_view id() const override { return "remove_first"; }
    std::string_view type() const override { return id(); }
    EventDecision select(Event& event) const override {
        event.hits.erase(event.hits.begin());
        return EventDecision::reject_dummy;
    }
};
int main() {
    auto chain=std::make_unique<CutChain>();
    auto topology=std::make_unique<TopologyEventSelector>(TopologySelectorConfig{2,1,100});
    auto dummy=std::make_unique<DummyEventSelector>(2);
    chain->add(std::move(topology)); chain->add(std::move(dummy));
    Event traverse{0,1,{Hit{0,0,0,0,{1000},false},Hit{0,0,1,0,{1000},false}}};
    Event single{0,1,{Hit{0,0,0,0,{1000},false}}};
    assert(apply_copy(*chain,traverse)==EventDecision::reject_through_going);
    assert(apply_copy(*chain,single)==EventDecision::keep);
    assert(apply_copy(*chain,traverse)==EventDecision::reject_through_going);
    assert(apply_copy(*chain,single)==EventDecision::reject_dummy);
    assert(apply_copy(*chain,single)==EventDecision::keep);
    auto rows=chain->summaries(true);
    assert(rows.size()==2 && rows[0].id=="topology" && rows[1].id=="dummy");
    assert(rows[0].evaluated_events==5 && rows[0].rejected_events==2);
    assert(rows[0].evaluated_hits==7 && rows[0].rejected_hits==4);
    assert(rows[1].evaluated_events==3 && rows[1].rejected_events==1);
    assert(rows[1].evaluated_hits==3 && rows[1].rejected_hits==1);
    assert(chain->summaries(true)[0].evaluated_events==0);
    assert(chain->summaries(false)[0].evaluated_events==5);
    auto bytes=banks::SelectionSummaryBank::encode(123,rows);
    assert(bytes.size()==24+2*72);
    banks::SelectionSummaryBank::Header header; banks::SelectionSummaryBank::Record record;
    std::memcpy(&header,bytes.data(),24); std::memcpy(&record,bytes.data()+24,72);
    assert(header.cycle_id==123 && header.magic==0x31535342 && header.cut_count==2);
    assert(std::string(record.id)=="topology" && record.enabled==1 && record.priority==100 && record.rejected_hits==4);
    chain->reset_counters();
    assert(chain->summaries(false)[0].evaluated_events==0);
    assert(apply_copy(*chain,single)==EventDecision::keep); // Dummy sequence resets.
    auto disabled=std::make_unique<DummyEventSelector>(1); disabled->set_enabled(false);
    assert(apply_copy(*disabled,single)==EventDecision::keep);
    assert(!disabled->summaries(false)[0].enabled && disabled->summaries(false)[0].evaluated_events==0);
    bool rejected=false;
    try {chain->add(std::make_unique<DummyEventSelector>(2));} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);

    CutChain reordered;
    auto first=std::make_unique<DummyEventSelector>(1); first->set_priority(-10);
    reordered.add(std::make_unique<TopologyEventSelector>(TopologySelectorConfig{2,1,100}));
    reordered.add(std::move(first));
    assert(apply_copy(reordered,traverse)==EventDecision::reject_dummy);
    auto ordered=reordered.summaries(false);
    assert(ordered[0].id=="dummy" && ordered[0].priority==-10 && ordered[0].evaluated_events==1);
    assert(ordered[1].id=="topology" && ordered[1].evaluated_events==0);
    CutChain ties;
    auto tie=std::make_unique<DummyEventSelector>(2); tie->set_priority(100);
    ties.add(std::make_unique<TopologyEventSelector>(TopologySelectorConfig{2,1,100})); ties.add(std::move(tie));
    assert(ties.summaries(false)[0].id=="dummy");

    // A buffered event rejected during EOR flush must still be counted.
    auto now=PeriodicReadoutEventBuilder::Clock::time_point{};
    EventBuilderPipeline pipeline(std::make_unique<testing::VectorInputStage>(single.hits),
        std::make_unique<PeriodicReadoutEventBuilder>(PeriodicReadoutEventBuilderConfig{1000},[&]{return now;}),
        std::make_unique<DummyEventSelector>(1),std::make_unique<ProductBankFinalizationStage>());
    assert(pipeline.run_once().empty());
    const auto final=pipeline.flush();
    assert(final.size()==1 && final[0].product_bank.size()==28);
    const auto decoded=banks::DataBank{}.decode(final[0].product_bank);
    assert(decoded.hits.empty() && decoded.start_ns==0 && decoded.end_ns==0);
    assert(pipeline.metrics().rejected_events==1);
    assert(pipeline.take_cut_summaries()[0].rejected_events==1);
    pipeline.reset_metrics(); assert(pipeline.cut_totals()[0].evaluated_events==0);
    CutChain partial;
    partial.add(std::make_unique<RemoveFirstHit>());
    partial.add(std::make_unique<DummyEventSelector>(2));
    auto payload=traverse;
    partial.apply(payload);
    assert(payload.hits.size()==1 && payload.start_ns==0 && payload.end_ns==1);
    auto partial_counts=partial.summaries(false);
    assert(partial_counts[0].evaluated_hits==2 && partial_counts[0].rejected_hits==1);
    assert(partial_counts[0].rejected_events==1);
    assert(partial_counts[1].evaluated_hits==1 && partial_counts[1].rejected_hits==0);

    now=PeriodicReadoutEventBuilder::Clock::time_point{};
    EventBuilderPipeline final_window(std::make_unique<testing::VectorInputStage>(single.hits),
        std::make_unique<PeriodicReadoutEventBuilder>(PeriodicReadoutEventBuilderConfig{1000},[&]{return now;}),
        std::make_unique<DummyEventSelector>(1),std::make_unique<ProductBankFinalizationStage>());
    assert(final_window.run_once().empty());
    now+=std::chrono::nanoseconds(2500);
    auto coalesced=final_window.drain_and_flush();
    assert(coalesced.size()==1 && coalesced[0].product_bank.size()==28);
    auto envelope=banks::DataBank{}.decode(coalesced[0].product_bank);
    assert(envelope.start_ns==0 && envelope.end_ns==2500 && envelope.hits.empty());

}
