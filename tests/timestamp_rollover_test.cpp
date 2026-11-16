#include "integration/sampic/collector/vendor_timestamp.h"
#include "integration/sampic/collector/extended_timestamp_parent.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data_verbose.h"
#include <iostream>

void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
using Clock=std::chrono::steady_clock;
auto arrival(std::int64_t ns){return Clock::time_point(std::chrono::nanoseconds(ns));}
std::shared_ptr<SampicEvent> parent(double time,double period,int ns){
    auto data=SampicEvent::makeEventStruct();data->NbOfHitsInEvent=1;data->Hit[0]={};
    data->Hit[0].FirstCellTimeStamp=time;data->Hit[0].AdvancedParams.PhysicalCell0TimeStamp=time-1;
    auto result=std::make_shared<SampicEvent>(std::move(data),SampicTimingBreakdown{},arrival(ns));
    result->setTimestampModulusNs(period);return result;
}
int main(){try{
    const double p=sampicTimestampModulusNs(6400);
    check(p==10995116277760.0 && sampicTimestampModulusNs(3200)==2*p,"40-bit modulus follows actual sampling frequency");
    // Reproduce the vendor's integer coarse/fine stitching around the 16-bit A wrap.
    const auto vendor_cell0=[](std::uint64_t fpga,std::uint64_t a){
        return static_cast<double>(64*(a+((fpga-a)&0xFFFFFF0000ULL)))*(156.25/1000);};
    const double t1=vendor_cell0(65535,65535),t2=vendor_cell0(65536,0);
    check(t2-t1==10,"vendor handles the short 16-bit rollover");
    SampicTimestampUnwrapper u;check(u.unwrap(t1,p,arrival(1))==t1 && u.unwrap(t2,p,arrival(2))==t2,"no double correction at 16-bit wrap");
    SampicTimestampUnwrapper wrap;
    check(wrap.unwrap(p-3,p,arrival(1))==p-3,"initial epoch unchanged");
    check(wrap.unwrap(-1,p,arrival(2))==p-1,"negative first-cell timestamp near FPGA wrap");
    check(wrap.unwrap(2,p,arrival(3))==p+2,"full counter wrap extended");
    check(wrap.unwrap(p-2,p,arrival(4))==p-2,"late previous-epoch hit retained in correct epoch");
    check(wrap.unwrap(3,p,arrival(5))==p+3,"late hit does not rewind epoch");
    wrap.unwrap(p/3,p,arrival(6));wrap.unwrap(2*p/3,p,arrival(7));
    check(wrap.unwrap(1,p,arrival(8))==2*p+1,"multiple full rollovers");
    SampicTimestampUnwrapper startup;check(startup.unwrap(-1,p,arrival(1))==-1 && startup.unwrap(p-2,p,arrival(2))==-2,"startup modulo borrow maps to negative epoch");
    SampicTimestampUnwrapper sparse;sparse.unwrap(1,p,arrival(0));bool failed=false;
    try{sparse.unwrap(2,p,arrival(static_cast<std::int64_t>(p/2)+1));}catch(const std::runtime_error&){failed=true;}
    check(failed,"ambiguous long silence must fail rather than invent an epoch");
    failed=false;try{u.unwrap(t2,2*p,arrival(3));}catch(const std::runtime_error&){failed=true;}
    check(failed,"mid-run sampling-frequency change must fail");
    SampicTimestampUnwrapper copied;auto pre=parent(p-5,p,1);extendParentTimestamps(pre,copied);
    auto post=parent(3,p,2);auto extended=extendParentTimestamps(post,copied);
    check(extended.hits[0].FirstCellTimeStamp==p+3 && extended.hits[0].AdvancedParams.PhysicalCell0TimeStamp==p+2,"stored first/physical timestamps agree");
    check(post->data()->Hit[0].FirstCellTimeStamp==3,"vendor buffer not mutated");
    std::weak_ptr<SampicEvent> weak=post;
    {
        FrontendEventBankDataVerbose bank({extended.owner},{extended.hits});
        extended.owner.reset();post.reset();check(!weak.expired(),"verbose bank retains adjusted hit owner");
        std::vector<std::uint8_t> payload(bank.size());bank.writeTo(payload.data());double stored;
        std::memcpy(&stored,payload.data()+payload.size()-8,8);check(stored==p+3,"verbose timestamp serialized after original owner release");
    }
    check(weak.expired(),"aliasing ownership released");
    std::cout<<"Vendor 16-bit reconstruction, full/multiple rollovers, late epochs, ambiguity and adjusted-bank lifetime passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
