#include "sampic_event_builder/pipeline/event_builder_pipeline.h"
#include "sampic_event_builder/stages/building/time_window_event_builder.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include "sampic_event_builder/stages/selection/passthrough_selection_stage.h"
#include "sampic_event_builder/stages/finalization/product_bank_finalization_stage.h"
#include "sampic_event_builder/serialization/product_bank_decoder.h"
#include "sampic_event_builder/testing/crate_simulator.h"
#include "sampic_event_builder/testing/vector_input_stage.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <map>
#include <tuple>
using namespace sampic::event_builder;
int main(int argc, char** argv) {
    std::size_t crates=2, count=100000, samples=64, window=200;
    bool select=false;
    for (int i=1; i<argc; ++i) {
        const std::string name=argv[i];
        if (i+1==argc) throw std::invalid_argument("missing argument value");
        const auto value=std::stoull(argv[++i]);
        if(name=="--crates") crates=value;
        else if(name=="--hits") count=value;
        else if(name=="--samples") samples=value;
        else if(name=="--window-ns") window=value;
        else if(name=="--select") select=value!=0;
        else throw std::invalid_argument("unknown argument: "+name);
    }
    std::vector<Hit> hits;
    for(std::size_t crate=0; crate<crates; ++crate) {
        auto generated=CrateSimulator({static_cast<std::uint16_t>(crate),256,
            static_cast<std::uint32_t>(samples),100000.0,0.10,crate+1}).generate(count);
        hits.insert(hits.end(),generated.begin(),generated.end());
    }
    std::ranges::sort(hits,{},&Hit::timestamp_ns);
    std::unique_ptr<SelectionStage> cuts=std::make_unique<PassthroughSelectionStage>();
    if(select) cuts=std::make_unique<TopologyEventSelector>(TopologySelectorConfig{8,32,3000});
    EventBuilderPipeline pipeline(std::make_unique<testing::VectorInputStage>(hits),
        std::make_unique<TimeWindowEventBuilder>(window),std::move(cuts),
        std::make_unique<ProductBankFinalizationStage>());
    const auto output=pipeline.run_once();
    std::map<std::tuple<std::uint16_t,std::uint16_t,std::uint16_t,std::uint64_t>, std::vector<std::int16_t>> originals;
    for(const auto& hit:hits) originals[{hit.crate,hit.board,hit.channel,hit.timestamp_ns}]=hit.waveform;
    std::size_t output_bytes=0, retained=0;
    for(const auto& event:output) {
        output_bytes+=event.product_bank.size();
        const auto decoded=ProductBankDecoder{}.decode(event.product_bank);
        for(const auto& hit:decoded.hits) {
            const auto original=originals.find({hit.crate,hit.board,hit.channel,hit.timestamp_ns});
            if(original==originals.end() || original->second!=hit.waveform)
                throw std::runtime_error("waveform round-trip failed");
            ++retained;
        }
    }
    const auto& m=pipeline.metrics();
    std::cout<<"crates="<<crates<<" hits="<<hits.size()<<" retained="<<retained<<" select="<<select<<'\n';
    std::cout<<"raw_bytes="<<hits.size()*samples*2<<" product_bytes="<<output_bytes<<'\n';
    std::cout<<"stage items/s MB/s elapsed_ms\n"<<std::fixed<<std::setprecision(2);
    for(const auto& entry : {std::pair{"collection",m.event_building},
                            std::pair{"cuts",m.selection},std::pair{"finalization",m.finalization}}) {
        const auto& stage=entry.second;
        const double seconds=stage.processing_nanoseconds/1e9;
        std::cout<<entry.first<<' '<<(seconds?stage.hits/seconds:0)<<' '
            <<(seconds?stage.output_bytes/seconds/1e6:0)<<' '<<seconds*1000<<'\n';
    }
}
