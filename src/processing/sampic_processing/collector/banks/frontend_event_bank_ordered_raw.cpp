#include "processing/sampic_processing/collector/banks/frontend_event_bank_ordered_raw.h"

#include "SAMPIC_256Ch_Type.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
template <typename T>
void append(std::vector<std::uint8_t>& output, const T& value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    output.insert(output.end(), bytes, bytes + sizeof(T));
}
} // namespace

FrontendEventBankOrderedRaw::FrontendEventBankOrderedRaw(
    const std::vector<const HitStruct*>& hits) {
    setBankPrefix("SR");

    Header header{kMagic, kVersion, sizeof(Header), 0};
    append(payload_, header);

    for (const HitStruct* hit : hits) {
        if (hit == nullptr) {
            continue;
        }

        const int bounded_samples = std::clamp(
            hit->DataSize, 0, static_cast<int>(MAX_NB_OF_SAMPLES));
        const auto sample_count = static_cast<std::uint16_t>(bounded_samples);
        const std::size_t record_size =
            sizeof(HitHeader) + sample_count * sizeof(std::uint16_t);
        if (record_size > std::numeric_limits<std::uint32_t>::max()) {
            throw std::overflow_error("ordered raw SAMPIC record is too large");
        }

        const HitHeader record{
            static_cast<std::uint32_t>(record_size),
            static_cast<std::int16_t>(hit->FeBoardIndex),
            static_cast<std::int16_t>(hit->SampicIndex),
            static_cast<std::int16_t>(hit->ChannelIndex),
            static_cast<std::int16_t>(hit->Channel),
            hit->HitNumber,
            hit->CellInfo,
            hit->FirstCellPhysicalIndex,
            sample_count,
            0,
            hit->TimeInstant,
            hit->FirstCellTimeStamp};
        append(payload_, record);

        const auto* sample_bytes = reinterpret_cast<const std::uint8_t*>(
            hit->OrderedRawDataSamples);
        payload_.insert(
            payload_.end(), sample_bytes,
            sample_bytes + sample_count * sizeof(std::uint16_t));
        ++header.hit_count;
    }

    std::memcpy(payload_.data(), &header, sizeof(header));
}
