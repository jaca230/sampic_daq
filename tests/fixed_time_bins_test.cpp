#include "processing/sampic_processing/collector/modes/periodic_readout/periodic_readout_mode.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_readout_tick.h"
#include "integration/sampic/collector/vendor_timestamp.h"
#include "processing/sampic_processing/collector/modes/fixed_time_bins/fixed_time_bins_mode.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_time_bin.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data_verbose.h"
#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "core/config/memory_config_store.h"
#include <thread>
#include <algorithm>
#include <iostream>

void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
std::shared_ptr<SampicEvent> parent(std::initializer_list<double> timestamps) {
    static int sequence = 0;
    auto data = SampicEvent::makeEventStruct();
    data->NbOfHitsInEvent = timestamps.size();
    int i = 0;
    for (double timestamp : timestamps) {
        auto& hit = data->Hit[i++]; hit = {};
        hit.FirstCellTimeStamp = timestamp; hit.DataSize = 1;
        hit.CorrectedDataSamples[0] = 0.1; hit.ADCCorrected = TRUE;
    }
    return std::make_shared<SampicEvent>(std::move(data), SampicTimingBreakdown{},
        std::chrono::steady_clock::time_point(std::chrono::nanoseconds(++sequence)));
}
using Record = FrontendEventBankTimeBin::Record;
Record metadata(const std::shared_ptr<FrontendEvent>& event) {
    auto* bank = event->findBankByPrefix("SB"); check(bank != nullptr, "bin metadata missing");
    Record r; std::memcpy(&r, bank->data(), sizeof(r));
    check(r.format_version == 1 && r.record_size == 32, "bin metadata version/size");
    return r;
}
std::vector<Record> pop(FrontendEventBuffer& buffer) {
    std::vector<Record> records;
    while (auto event = buffer.pop()) records.push_back(metadata(*event));
    return records;
}
struct Fixture {
    SampicEventBuffer input{64}; FrontendEventBuffer output;
    FrontendCollectorDiagnosticsConfig diagnostics_config;
    frontend::collector::FrontendDiagnostics diagnostics{diagnostics_config};
    FrontendCollectorModeContext context{input, output, diagnostics_config, diagnostics};
    explicit Fixture(std::size_t capacity=128): output(capacity) {}
};
FrontendCollectorModeFixedTimeBinsConfig config() {
    FrontendCollectorModeFixedTimeBinsConfig c;
    c.allowed_lateness_ns = 0; c.wait_timeout_ms = 1; c.idle_flush_ms = 1000;
    return c;
}
int main(int argc, char** argv) {
    try {
        if (argc>1 && std::string(argv[1])=="--benchmark-periodic") {
            spdlog::set_level(spdlog::level::off);
            Fixture f;FrontendCollectorModePeriodicReadoutConfig c;
            FrontendCollectorModePeriodicReadout mode(f.context,c);mode.onStart();
            const auto start=std::chrono::steady_clock::now();
            std::vector<std::uint64_t> lateness;
            std::uint64_t ticks=0,coalesced=0;
            const auto consume=[&]{
                while(auto event=f.output.pop()) {
                    FrontendEventBankReadoutTick::Record r;
                    std::memcpy(&r,(*event)->findBankByPrefix("SR")->data(),sizeof(r));
                    if(!(r.flags&4)){
                        lateness.push_back(r.actual_readout_ns-(r.start_ns+c.readout_period_ns));
                        ticks+=r.ticks_elapsed;if(r.flags&2)++coalesced;
                    }
                }
            };
            do{mode.collect();consume();}while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<1);
            const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            mode.flush();consume();check(!lateness.empty(),"no periodic readouts in benchmark");std::sort(lateness.begin(),lateness.end());
            std::cout<<"duration_s,readouts,scheduled_ticks,coalesced_readouts,readouts_per_s,lateness_p50_ns,lateness_p99_ns,lateness_max_ns\n"
                <<elapsed<<','<<lateness.size()<<','<<ticks<<','<<coalesced<<','<<lateness.size()/elapsed<<','
                <<lateness[lateness.size()/2]<<','<<lateness[lateness.size()*99/100]<<','<<lateness.back()<<'\n';
            return 0;
        }

        {
            Fixture f; auto c = config(); FrontendCollectorModeFixedTimeBins mode(f.context,c);
            f.input.push(parent({9999,0,10000,20000,10001})); mode.collect();
            auto r = pop(f.output);
            check(r.size()==2 && r[0].bin_index==0 && r[0].hit_count==2 && r[1].hit_count==2, "half-open boundaries, unordered input");
            check(r[0].start_ns==0 && r[0].end_ns==10000 && r[1].start_ns==10000, "fixed grid");
            mode.flush(); r=pop(f.output);check(r.size()==1 && r[0].bin_index==2 && r[0].hit_count==1, "tail flushed");
            mode.flush();check(f.output.empty(), "flush idempotent");
        }
        {
            Fixture f; auto c=config();FrontendCollectorModeFixedTimeBins mode(f.context,c);
            f.input.push(parent({1000,35000}));mode.collect();auto r=pop(f.output);
            check(r.size()==3 && r[0].hit_count==1 && r[1].hit_count==0 && r[2].hit_count==0,"empty bins emitted");
            mode.flush(); r=pop(f.output);check(r.size()==1 && r[0].bin_index==3,"last occupied bin");
        }
        {
            Fixture f;auto c=config();c.allowed_lateness_ns=20000;FrontendCollectorModeFixedTimeBins mode(f.context,c);
            f.input.push(parent({25000}));mode.collect();check(f.output.empty(),"reordering grace");
            f.input.push(parent({15000,1000}));mode.collect();check(f.output.empty(),"earlier FEB accepted before closure");
            f.input.push(parent({45000}));mode.collect();auto r=pop(f.output);
            check(r.size()==2 && r[0].bin_index==0 && r[1].bin_index==1,"watermark closure");
            f.input.push(parent({12000}));bool failed=false;
            try {mode.collect();} catch(const std::runtime_error&) {failed=true;}
            check(failed,"too-late hit must not be silently split into a second bin");
        }
        {
            Fixture f;auto c=config();c.allowed_lateness_ns=1e6;c.idle_flush_ms=1;FrontendCollectorModeFixedTimeBins mode(f.context,c);
            f.input.push(parent({1}));mode.collect();check(f.output.empty(),"tail pending until idle");
            std::this_thread::sleep_for(std::chrono::milliseconds(3));mode.collect();
            check(pop(f.output).size()==1,"idle timeout works without new input");
        }
        {
            Fixture f;auto c=config();c.bin_origin_ns=1000;c.bin_width_ns=2500;c.emit_empty_bins=false;c.verbose_banks=true;
            FrontendCollectorModeFixedTimeBins mode(f.context,c);f.input.push(parent({1000,11000}));mode.collect();
            auto e=f.output.pop();check(e && metadata(*e).bin_index==0,"configurable origin");
            check(dynamic_cast<FrontendEventBankDataVerbose*>((*e)->findBankByPrefix("SD"))!=nullptr,"verbose toggle inherited");
            check(f.output.empty(),"optional empty suppression");mode.flush();auto r=pop(f.output);
            check(r.size()==1 && r[0].start_ns==11000,"configurable width");
        }
        {
            Fixture f;auto c=config();FrontendCollectorModeFixedTimeBins mode(f.context,c);
            mode.collect();mode.flush();check(f.output.empty(),"no fictitious run clock before first hit");
            f.input.push(parent({std::numeric_limits<double>::quiet_NaN()}));bool failed=false;
            try {mode.collect();}catch(const std::runtime_error&){failed=true;}check(failed,"nonfinite timestamps rejected");
        }
        {
            // Real manager drain with a two-event output queue and an eleven-bin gap.
            SampicEventBuffer input(8);MemoryConfigStore store;
            FrontendEventCollector::initializeOdb(store,"/modes");
            auto json=store.readJson("/modes/fixed_time_bins");
            check(json.at("bin_width_ns")==10000 && json.at("emit_empty_bins")==true,"registry ODB defaults");
            json["bin_width_ns"]=10;json["allowed_lateness_ns"]=0;json["wait_timeout_ms"]=1;json["max_bins_per_collect"]=2;
            store.writeJson("/modes/fixed_time_bins",json);
            FrontendEventCollectorConfig cfg;cfg.mode="fixed_time_bins";cfg.buffer_size=2;
            FrontendEventCollector collector(input,cfg,store,"/modes");
            input.push(parent({0,100}));std::vector<Record> all;
            check(collector.drain(100,[&]{auto r=pop(collector.buffer());all.insert(all.end(),r.begin(),r.end());return true;}),"incremental EOR drain");
            auto rest=pop(collector.buffer());all.insert(all.end(),rest.begin(),rest.end());
            check(all.size()==11,"backpressure must not drop bins");
            for(int i=0;i<11;++i)check(all[i].bin_index==i && all[i].hit_count==((i==0||i==10)?1:0),"contiguous drained indices");
        }
        {
            Fixture f;auto c=config();c.max_bins_per_collect=1;FrontendCollectorModeFixedTimeBins mode(f.context,c);
            auto p=parent({0,1000,10000});std::weak_ptr<SampicEvent> owner=p;f.input.push(p);p.reset();
            mode.collect();check(!owner.expired(),"pending hit owners retained after pruning");
            mode.flush();auto r=pop(f.output);check(r.size()==2 && r[0].hit_count==2,"parent shared across bins");
            check(owner.expired(),"compact mode releases parents after serialization");
        }
        {
            Fixture f;auto c=config();c.allowed_lateness_ns=20000;FrontendCollectorModeFixedTimeBins mode(f.context,c);
            const double period=sampicTimestampModulusNs(6400);
            for(double time:{period-5000,1000.0,period-1000,3000.0}) {
                auto e=parent({time});e->setTimestampModulusNs(period);
                e->data()->Hit[0].AdvancedParams.PhysicalCell0TimeStamp=time-1;f.input.push(e);
            }
            mode.collect();mode.flush();std::size_t hits=0,events=0;
            while(auto e=f.output.pop()) {
                const auto bin=metadata(*e);auto* data=(*e)->findBankByPrefix("SD");
                for(std::size_t i=0;i<bin.hit_count;++i) {
                    FrontendEventBankData::Record r;
                    std::memcpy(&r,data->data()+12+i*sizeof(r),sizeof(r));
                    check(r.first_cell_timestamp_ns>=bin.start_ns && r.first_cell_timestamp_ns<bin.end_ns,"rollover hit matches SB interval");
                }
                hits+=bin.hit_count;++events;
            }
            check(events==2 && hits==4,"rollover must not create hours of spurious empty bins");
        }
        {
            Fixture f;FrontendCollectorModePeriodicReadoutConfig c;
            auto time=std::chrono::steady_clock::time_point{};
            FrontendCollectorModePeriodicReadout mode(f.context,c,[&]{return time;});mode.onStart();
            const auto tick=[&] {
                auto event=f.output.pop();check(event.has_value(),"periodic event missing");
                FrontendEventBankReadoutTick::Record r;
                auto* bank=(*event)->findBankByPrefix("SR");check(bank!=nullptr,"tick metadata missing");
                std::memcpy(&r,bank->data(),sizeof(r));return r;
            };
            time+=std::chrono::nanoseconds(10000);mode.collect();auto r=tick();
            check(r.hit_count==0 && r.start_ns==0 && r.end_ns==10000 && r.ticks_elapsed==1,"empty tick before first hardware hit");
            f.input.push(parent({123456789}));time+=std::chrono::nanoseconds(1);mode.collect();check(f.output.empty(),"timer independent of hit timestamp");
            time=std::chrono::steady_clock::time_point(std::chrono::nanoseconds(20000));mode.collect();r=tick();
            check(r.hit_count==1 && r.first_tick==1,"drain arrived hits at next tick");
            time=std::chrono::steady_clock::time_point(std::chrono::nanoseconds(55000));mode.collect();r=tick();
            check(r.ticks_elapsed==3 && r.end_ns==50000 && r.actual_readout_ns==55000 && (r.flags&2),"late software tick explicitly coalesced");
            time+=std::chrono::nanoseconds(2000);f.input.push(parent({100}));mode.collect();mode.flush();r=tick();
            check(r.hit_count==1 && r.start_ns==50000 && r.end_ns==57000 && (r.flags&4),"partial final readout");
            time+=std::chrono::seconds(1);mode.flush();check(f.output.empty()&&!mode.hasPendingOutput(),"flush must not invent more ticks on each call");
        }
        {
            Fixture f(1);FrontendCollectorModePeriodicReadoutConfig c;
            auto time=std::chrono::steady_clock::time_point{};
            FrontendCollectorModePeriodicReadout mode(f.context,c,[&]{return time;});mode.onStart();
            for(int i=0;i<3;++i){time+=std::chrono::nanoseconds(10000);mode.collect();}
            mode.flush();check(mode.hasPendingOutput(),"periodic backpressure retained");
            int index=0;
            do {
                while(auto event=f.output.pop()) {
                    FrontendEventBankReadoutTick::Record r;
                    std::memcpy(&r,(*event)->findBankByPrefix("SR")->data(),sizeof(r));
                    check(r.first_tick==static_cast<std::uint64_t>(index++),"periodic readout order under pressure");
                }
                mode.flush();
            }while(mode.hasPendingOutput()||!f.output.empty());
            check(index==3,"periodic output queue must not drop readouts");
        }
        {
            FrontendEventBuffer queue(1);
            auto older=std::make_shared<FrontendEvent>();auto newer=std::make_shared<FrontendEvent>();
            check(queue.tryPush(newer),"queue insertion");queue.pushFront(older);
            check(queue.size()==2 && *queue.pop()==older && *queue.pop()==newer,"returning in-flight readout must preserve queued event");
        }
        for (bool verbose : {false,true}) {
            Fixture f;FrontendCollectorModePeriodicReadoutConfig c;c.verbose_banks=verbose;
            auto time=std::chrono::steady_clock::time_point{};
            FrontendCollectorModePeriodicReadout mode(f.context,c,[&]{return time;});mode.onStart();
            const double period=sampicTimestampModulusNs(6400);
            for (double raw : {period-5,3.0}) {
                auto event=parent({raw});event->setTimestampModulusNs(period);
                event->data()->Hit[0].AdvancedParams.PhysicalCell0TimeStamp=raw-1;
                f.input.push(event);
            }
            time+=std::chrono::nanoseconds(10000);mode.collect();auto event=f.output.pop();
            check(event.has_value(),"periodic rollover readout missing");
            auto* sd=(*event)->findBankByPrefix("SD");auto* sh=(*event)->findBankByPrefix("SH");
            std::vector<std::uint8_t> data(sd->size());sd->writeTo(data.data());
            for(int i=0;i<2;++i){
                const double expected=i==0?period-5:3;
                double first,physical;
                if(verbose){
                    std::memcpy(&first,data.data()+i*344+336,8);
                    std::memcpy(&physical,sh->data()+8+i*54+10,8);
                }else{
                    FrontendEventBankData::Record r;
                    std::memcpy(&r,data.data()+12+i*sizeof(r),sizeof(r));first=r.first_cell_timestamp_ns;
                    std::memcpy(&physical,sh->data()+8+i*32,8);
                }
                check(first==expected && physical==expected-1,"periodic mode must preserve vendor timestamps across rollover");
            }
        }
        for (int variant=0;variant<5;++variant) {
            auto c=config();if(variant==0)c.bin_width_ns=0;if(variant==1)c.allowed_lateness_ns=-1;
            if(variant==2)c.bin_bank_prefix="SD";if(variant==3)c.max_bins_per_collect=0;
            if(variant==4)c.bin_origin_ns=std::numeric_limits<double>::infinity();
            bool failed=false;try{FrontendCollectorModeFixedTimeBins::validate(c);}catch(const std::invalid_argument&){failed=true;}
            check(failed,"invalid mode config rejected");
        }
        std::cout<<"Fixed bins: boundaries, empty events, reordering, idle flush, registry, verbose banks and bounded EOR drain passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
