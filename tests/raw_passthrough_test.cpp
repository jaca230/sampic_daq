#include "processing/sampic_processing/collector/banks/frontend_event_bank_raw.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_advanced_hits.h"
#include "sampic_event_builder/stages/input/midas_raw_input_stage.h"
#include "sampic_event_builder/stages/building/periodic_readout_event_builder.h"
#include "sampic_event_builder/banks/data_bank.h"
#include "sampic_event_builder/stages/selection/topology_selection_stage.h"
#include <cassert>
#include <cstring>
#include <limits>

using namespace sampic::event_builder;
int main() {
    HitStruct vendor{};
    vendor.FeBoardIndex=2; vendor.Channel=47; vendor.SampicIndex=2; vendor.ChannelIndex=15;
    vendor.HitNumber=123; vendor.DataSize=64; vendor.INLCorrected=TRUE;
    vendor.ADCCorrected=TRUE; vendor.ResidualPedestalCorrected=TRUE;
    vendor.CellInfo=63; vendor.FirstCellPhysicalIndex=61;
    vendor.RawTOTValue=1234; vendor.TOTValue=12.375f;
    vendor.FirstCellTimeStamp=123456.875; vendor.TimeInstant=123455.125;
    vendor.Amplitude=1.25f; vendor.Baseline=2.5f; vendor.Peak=3.75f;
    vendor.TimeIndex=4.5f; vendor.TimeAmplitude=5.125f;
    auto& a=vendor.AdvancedParams;
    a.SampicDataHeader=567; a.FirstTriggerPositionCell=63; a.TriggerPositionCell=61;
    a.PhysicalCell0TimeStamp=123450.0625; a.SampicTimeStampA=12345; a.SampicTimeStampB=12346;
    a.FPGATimeStamp=(1ULL<<60)+3; a.ADCCounter_LatchedAtEndOfConv=124;
    a.TimePhysicalIndex=22; a.StartOfADCRamp=555;
    for (int i=0; i<64; ++i) {
        vendor.RawDataSamples[i]=i+1000; vendor.OrderedRawDataSamples[i]=i+2000;
        vendor.CorrectedDataSamples[i]=(i-32)*0.0001f;
        a.TriggerPosition[i]=(i%3==0);
    }
    const auto check=[&] {
        auto event=SampicEvent::makeEventStruct();
        event->NbOfHitsInEvent=1;
        event->Hit[0]=vendor;
        auto parent=std::make_shared<SampicEvent>(std::move(event),SampicTimingBreakdown{},
                                                std::chrono::steady_clock::time_point{});
        std::weak_ptr<SampicEvent> weak_parent=parent;
        FrontendEventBankRaw raw(parent);
        parent.reset();
        assert(!weak_parent.expired()); // Queued bank retains original vendor storage.
        assert(raw.data()==nullptr);
        assert(raw.size()==sizeof(sampic::wire::RawBankHeader)+sizeof(HitStruct));
        std::vector<std::uint8_t> payload(raw.size());
        raw.writeTo(payload.data());
        assert(std::memcmp(payload.data()+sizeof(sampic::wire::RawBankHeader),&vendor,sizeof(vendor))==0);
        MidasRawInputStage input;
        input.ingest("SR01",payload);
        auto hits=input.read_available();
        assert(hits.size()==1 && hits[0].crate==1 && hits[0].timestamp_ns==123456);
        // Even fields intentionally omitted from SD/SH survive raw transport.
        assert(hits[0].waveform.empty()); // No early quantization or sample-vector copy.
        auto original_payload=payload;
        std::fill(payload.begin(),payload.end(),0); // Simulate MIDAS receive-buffer reuse.
        assert(std::memcmp(hits[0].raw.get(),&vendor,sizeof(vendor))==0);
        const auto& received=*hits[0].raw;
        assert(received.Amplitude==vendor.Amplitude && received.Baseline==vendor.Baseline);
        assert(received.Peak==vendor.Peak && received.TimeIndex==vendor.TimeIndex);
        assert(received.TimeInstant==vendor.TimeInstant && received.TimeAmplitude==vendor.TimeAmplitude);
        assert(received.AdvancedParams.SampicDataHeader==a.SampicDataHeader);
        assert(received.AdvancedParams.FirstTriggerPositionCell==a.FirstTriggerPositionCell);
        assert(received.AdvancedParams.TriggerPositionCell==a.TriggerPositionCell);
        assert(received.AdvancedParams.ADCCounter_LatchedAtEndOfConv==a.ADCCounter_LatchedAtEndOfConv);
        assert(received.AdvancedParams.TimePhysicalIndex==a.TimePhysicalIndex);
        assert(received.AdvancedParams.StartOfADCRamp==a.StartOfADCRamp);
        for (int i=0; i<64; ++i) {
            assert(received.RawDataSamples[i]==vendor.RawDataSamples[i]);
            assert(received.OrderedRawDataSamples[i]==vendor.OrderedRawDataSamples[i]);
            assert(std::memcmp(&received.CorrectedDataSamples[i],&vendor.CorrectedDataSamples[i],sizeof(float))==0);
            assert(received.AdvancedParams.TriggerPosition[i]==a.TriggerPosition[i]);
        }
        using Clock=PeriodicReadoutEventBuilder::Clock;
        auto now=Clock::time_point{};
        PeriodicReadoutEventBuilder collector({1000},[&]{return now;});
        assert(collector.build(std::move(hits)).empty());
        now+=std::chrono::nanoseconds(1000);
        auto events=collector.build({}); assert(events.size()==1);
        auto bytes=banks::DataBank{}.encode(events[0]);
        assert(bytes.size()==28+188);
        banks::DataBank::Record record{};
        std::memcpy(&record,bytes.data()+28,sizeof(record));
        FrontendEventBankData sd({&vendor});
        FrontendEventBankAdvancedHits sh({&vendor});
        assert(std::memcmp(&record.data,sd.data()+sizeof(FrontendEventBankData::Header),154)==0);
        assert(std::memcmp(&record.advanced,sh.data()+sizeof(FrontendEventBankAdvancedHits::Header),32)==0);
        assert(record.data.first_cell_timestamp_ns==123456.875);
        assert(record.advanced.physical_cell0_timestamp_ns==123450.0625);
        assert(record.data.hit_number==123 && record.data.raw_tot==std::min(vendor.RawTOTValue,65535));
        assert(record.data.calibrated_tot_ns==12.375f);
        assert(record.advanced.fpga_timestamp==(1ULL<<60)+3);
        std::uint64_t mask=0;
        for (int i=0;i<64;i+=3) mask|=1ULL<<i;
        assert(record.advanced.trigger_position_mask==mask);
        if (vendor.ADCCorrected) {
            assert(record.data.waveform[0]==-32 && record.data.flags==7);
        } else {
            assert(record.data.waveform[0]==1235 && record.data.waveform[1]==32767);
            assert(record.data.waveform[2]==0 && record.data.flags==61);
        }
        auto decoded=banks::DataBank{}.decode(bytes);
        assert(decoded.hits[0].compact->data.first_cell_timestamp_ns==vendor.FirstCellTimeStamp);
        assert(banks::DataBank{}.encode(decoded)==bytes);
        assert(decoded.hits[0].waveform[0]!=vendor.OrderedRawDataSamples[0]);
        return original_payload;
    };
    auto bank=check();
    // ADC counts when ADC correction is disabled, with identical SD quality flags.
    vendor.ADCCorrected=FALSE;
    vendor.CorrectedDataSamples[0]=1234.5f;
    vendor.CorrectedDataSamples[1]=40000.f;
    vendor.CorrectedDataSamples[2]=std::numeric_limits<float>::quiet_NaN();
    vendor.RawTOTValue=70000;
    check();
    // All 64 elements survive even when only a subset is valid.
    vendor.DataSize=4; check();
    auto rejects=[](const auto& action) {
        try {action();} catch(const std::exception&) {return true;} return false;
    };
    MidasRawInputStage input;
    for (std::size_t length=0; length<bank.size(); ++length)
        assert(rejects([&]{input.ingest("SR00",{bank.data(),length});}));
    auto malformed=std::vector<std::uint8_t>(bank.data(),bank.data()+bank.size());
    malformed.push_back(0);
    assert(rejects([&]{input.ingest("SR00",malformed);}));
    malformed.pop_back(); malformed[4]=1; // reject old/incomplete SR layout
    assert(rejects([&]{input.ingest("SR00",malformed);}));
    assert(input.hits_received()==0 && input.read_available().empty());
    auto empty_event=SampicEvent::makeEventStruct();
    empty_event->NbOfHitsInEvent=0;
    auto empty_parent=std::make_shared<SampicEvent>(std::move(empty_event),SampicTimingBreakdown{},
                                                 std::chrono::steady_clock::time_point{});
    FrontendEventBankRaw empty(empty_parent);
    std::vector<std::uint8_t> empty_payload(empty.size()); empty.writeTo(empty_payload.data());
    input.ingest("SR00",empty_payload);
    assert(input.read_available().empty());

    // Two hits share one batch allocation/control block. Keeping either hit
    // alive must retain the batch after the input stage and its buffer disappear.
    std::vector<Hit> batch_hits;
    {
        auto event=SampicEvent::makeEventStruct(); event->NbOfHitsInEvent=2;
        event->Hit[0]=vendor; event->Hit[1]=vendor;
        event->Hit[1].Channel=48; event->Hit[1].SampicIndex=3; event->Hit[1].ChannelIndex=0;
        auto parent=std::make_shared<SampicEvent>(std::move(event),SampicTimingBreakdown{},
                                                std::chrono::steady_clock::time_point{});
        FrontendEventBankRaw raw(parent);
        std::vector<std::uint8_t> payload(raw.size()); raw.writeTo(payload.data());
        MidasRawInputStage source; source.ingest("SR00",payload);
        batch_hits=source.read_available();
    }
    assert(batch_hits.size()==2 && batch_hits[1].raw.get()==batch_hits[0].raw.get()+1);
    assert(!batch_hits[0].raw.owner_before(batch_hits[1].raw));
    assert(!batch_hits[1].raw.owner_before(batch_hits[0].raw));
    std::weak_ptr<const HitStruct> lifetime=batch_hits[0].raw;
    auto retained=std::move(batch_hits[1]); batch_hits.clear();
    assert(!lifetime.expired() && retained.raw->Channel==48 && retained.waveform.empty());
    assert(retained.sample_count()==4 && retained.corrected_sample(0)==1234.5);
    // The optional cut reads vendor samples even though the waveform vector is empty.
    Event cut_event{0,1,{retained,retained}};
    cut_event.hits[0].board=0; cut_event.hits[0].channel=0;
    cut_event.hits[1].board=0; cut_event.hits[1].channel=1;
    assert(TopologyEventSelector({2,1,1'000'000}).select(cut_event)==EventDecision::reject_through_going);
    cut_event.hits.clear(); retained=Hit{};
    assert(lifetime.expired());
}
