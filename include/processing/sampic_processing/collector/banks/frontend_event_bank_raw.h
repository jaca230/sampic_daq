#pragma once
#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "processing/sampic_processing/collector/banks/sampic_hit_format.h"
#include "integration/sampic/collector/sampic_event.h"
#include <memory>

// Own the vendor event until MIDAS serialization. No intermediate hit payload.
// This internal transport requires matching vendor HitStruct layouts at both ends.
class FrontendEventBankRaw final : public FrontendEventBank {
public:
    using Header = sampic::wire::RawBankHeader;
    using Record = HitStruct;
    static constexpr auto kMagic = sampic::wire::raw_magic;
    static constexpr auto kVersion = sampic::wire::raw_version;
    explicit FrontendEventBankRaw(std::shared_ptr<const SampicEvent> parent);
    const std::uint8_t* data() const override { return nullptr; } // header + vendor slice
    std::size_t size() const override { return sizeof(Header)+header_.hit_count*sizeof(Record); }
    void writeTo(std::uint8_t* destination) const override;
private:
    std::shared_ptr<const SampicEvent> parent_;
    Header header_{};
};
