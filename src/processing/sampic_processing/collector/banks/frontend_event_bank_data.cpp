#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

static_assert(std::endian::native == std::endian::little, "MIDAS compact banks require little-endian serialization");

FrontendEventBankData::FrontendEventBankData(const std::vector<const HitStruct*>& hits) {
    setBankPrefix("SD");
    const auto count = std::count_if(hits.begin(), hits.end(), [](auto* hit) { return hit != nullptr; });
    if (count > std::numeric_limits<std::uint32_t>::max() ||
        static_cast<std::size_t>(count) > (payload_.max_size() - sizeof(Header)) / sizeof(Record)) {
        throw std::length_error("too many hits for compact SD bank");
    }
    const Header header{{'S', 'D', 'C', '1'}, FORMAT_VERSION, sizeof(Record), static_cast<std::uint32_t>(count)};
    payload_.resize(sizeof(Header) + count * sizeof(Record));
    std::memcpy(payload_.data(), &header, sizeof(header));
    auto* destination = payload_.data() + sizeof(Header);
    for (const auto* hit : hits) {
        if (!hit) continue;
        const auto record = sampic::wire::compact_data(*hit);
        std::memcpy(destination, &record, sizeof(record));
        destination += sizeof(record);
    }
}
