#include "processing/sampic_processing/collector/banks/frontend_event_bank_raw.h"
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

static_assert(std::is_trivially_copyable_v<HitStruct>);
static_assert(MAX_NB_OF_SAMPLES==64);

FrontendEventBankRaw::FrontendEventBankRaw(std::shared_ptr<const SampicEvent> parent)
    : parent_(std::move(parent)) {
    if (!parent_ || !parent_->data())
        throw std::invalid_argument("raw bank requires an owned vendor event");
    const auto count=parent_->data()->NbOfHitsInEvent;
    if (count<0 || count>MAX_EXPECTED_FRAMES)
        throw std::invalid_argument("invalid raw SAMPIC hit count");
    setBankPrefix("SR");
    header_={kMagic,kVersion,sizeof(Header),sizeof(Record),static_cast<std::uint32_t>(count)};
}

void FrontendEventBankRaw::writeTo(std::uint8_t* destination) const {
    std::memcpy(destination,&header_,sizeof(header_));
    if (header_.hit_count)
        std::memcpy(destination+sizeof(header_),parent_->data()->Hit,header_.hit_count*sizeof(Record));
}
