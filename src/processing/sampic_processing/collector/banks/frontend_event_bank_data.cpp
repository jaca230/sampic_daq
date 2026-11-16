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
        const auto bounded = [](int value, int maximum) -> std::uint8_t {
            if (value < 0 || value > maximum) throw std::invalid_argument("invalid compact SD hit index/size");
            return static_cast<std::uint8_t>(value);
        };
        Record record{};
        record.feb_index = bounded(hit->FeBoardIndex, 3);
        record.channel = bounded(hit->Channel, 63);
        record.hit_number = hit->HitNumber;
        record.sampic_index = bounded(hit->SampicIndex, 3);
        record.sampic_channel_index = bounded(hit->ChannelIndex, 15);
        record.waveform_size = bounded(hit->DataSize, 64);
        record.cell_info = bounded(hit->CellInfo, 63);
        record.first_cell_physical_index = bounded(hit->FirstCellPhysicalIndex, 63);
        if (hit->INLCorrected) record.flags |= INL_CORRECTED;
        if (hit->ADCCorrected) record.flags |= ADC_CORRECTED;
        if (hit->ResidualPedestalCorrected) record.flags |= RESIDUAL_PEDESTAL_CORRECTED;
        const double scale = hit->ADCCorrected ? 10000.0 : 1.0;
        for (int index = 0; index < hit->DataSize; ++index) {
            const double value = static_cast<double>(hit->CorrectedDataSamples[index]) * scale;
            if (!std::isfinite(value)) {
                record.flags |= WAVEFORM_NONFINITE;
                continue; // Canonical zero; flag distinguishes it from a valid measurement.
            }
            const double rounded = std::round(value);
            if (rounded < -32768.0 || rounded > 32767.0) record.flags |= WAVEFORM_CLIPPED;
            record.waveform[index] = static_cast<std::int16_t>(std::clamp(rounded, -32768.0, 32767.0));
        }
        if (hit->RawTOTValue < 0 || hit->RawTOTValue > 65535) record.flags |= RAW_TOT_CLIPPED;
        record.raw_tot = static_cast<std::uint16_t>(std::clamp(hit->RawTOTValue, 0, 65535));
        record.calibrated_tot_ns = hit->TOTValue;
        record.first_cell_timestamp_ns = hit->FirstCellTimeStamp;
        std::memcpy(destination, &record, sizeof(record));
        destination += sizeof(record);
    }
}
