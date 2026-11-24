#pragma once
// Shared compact disk layouts and internal raw transport header. Compact records
// have fixed-width little-endian fields; raw records use the vendor HitStruct ABI.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <limits>

namespace sampic::wire {
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
#pragma pack(push, 1)
struct RawBankHeader {
    std::uint32_t magic;
    std::uint16_t version, header_size;
    std::uint32_t record_size, hit_count;
};
struct CompactData {
    std::uint8_t feb_index, channel;
    std::int32_t hit_number; // Vendor-event-local, not a run-global ID.
    std::uint8_t sampic_index, sampic_channel_index, waveform_size, flags;
    std::uint8_t cell_info, first_cell_physical_index;
    // ADC_CORRECTED: 0.1 mV/count (volts=value/10000); otherwise ADC counts.
    // Samples are rounded to int16, with clipping/nonfinite quality flags.
    std::int16_t waveform[64];
    std::uint16_t raw_tot;
    float calibrated_tot_ns;
    double first_cell_timestamp_ns;
};
struct CompactAdvanced {
    double physical_cell0_timestamp_ns;
    std::int32_t sampic_timestamp_a, sampic_timestamp_b;
    std::uint64_t fpga_timestamp, trigger_position_mask;
};
#pragma pack(pop)
inline constexpr std::uint32_t raw_magic = 0x33575253; // SRW3: native vendor HitStruct transport
inline constexpr std::uint16_t raw_version = 3;
enum Flags : std::uint8_t {
    INL_CORRECTED=1, ADC_CORRECTED=2, RESIDUAL_PEDESTAL_CORRECTED=4,
    WAVEFORM_CLIPPED=8, WAVEFORM_NONFINITE=16, RAW_TOT_CLIPPED=32
};
static_assert(sizeof(RawBankHeader)==16);
static_assert(sizeof(CompactData)==154);
static_assert(sizeof(CompactAdvanced)==32);

template<class T> void validate_hit(const T& hit) {
    if (hit.FeBoardIndex<0 || hit.FeBoardIndex>3 || hit.Channel<0 || hit.Channel>63 ||
        hit.SampicIndex<0 || hit.SampicIndex>3 || hit.ChannelIndex<0 || hit.ChannelIndex>15 ||
        hit.DataSize<0 || hit.DataSize>64 || hit.CellInfo<0 || hit.CellInfo>63 ||
        hit.FirstCellPhysicalIndex<0 || hit.FirstCellPhysicalIndex>63)
        throw std::invalid_argument("invalid compact SD hit index/size");
}

template<class T> CompactData compact_data(const T& hit) {
    validate_hit(hit);
    CompactData record{};
    record.feb_index = static_cast<std::uint8_t>(hit.FeBoardIndex);
    record.channel = static_cast<std::uint8_t>(hit.Channel);
    record.hit_number = hit.HitNumber;
    record.sampic_index = static_cast<std::uint8_t>(hit.SampicIndex);
    record.sampic_channel_index = static_cast<std::uint8_t>(hit.ChannelIndex);
    record.waveform_size = static_cast<std::uint8_t>(hit.DataSize);
    record.cell_info = static_cast<std::uint8_t>(hit.CellInfo);
    record.first_cell_physical_index = static_cast<std::uint8_t>(hit.FirstCellPhysicalIndex);
    if (hit.INLCorrected) record.flags |= INL_CORRECTED;
    if (hit.ADCCorrected) record.flags |= ADC_CORRECTED;
    if (hit.ResidualPedestalCorrected) record.flags |= RESIDUAL_PEDESTAL_CORRECTED;
    const double scale = hit.ADCCorrected ? 10000.0 : 1.0;
    for (int i=0; i<hit.DataSize; ++i) {
        const double value = static_cast<double>(hit.CorrectedDataSamples[i])*scale;
        if (!std::isfinite(value)) { record.flags |= WAVEFORM_NONFINITE; continue; }
        const double rounded = std::round(value);
        if (rounded < -32768.0 || rounded > 32767.0) record.flags |= WAVEFORM_CLIPPED;
        record.waveform[i] = static_cast<std::int16_t>(std::clamp(rounded,-32768.0,32767.0));
    }
    if (hit.RawTOTValue < 0 || hit.RawTOTValue > 65535) record.flags |= RAW_TOT_CLIPPED;
    record.raw_tot = static_cast<std::uint16_t>(std::clamp(hit.RawTOTValue,0,65535));
    record.calibrated_tot_ns = hit.TOTValue;
    record.first_cell_timestamp_ns = hit.FirstCellTimeStamp;
    return record;
}

template<class T> CompactAdvanced compact_advanced(const T& hit) {
    const auto& a=hit.AdvancedParams;
    std::uint64_t mask=0;
    for (int i=0; i<64; ++i) if (a.TriggerPosition[i]) mask |= std::uint64_t{1}<<i;
    return {a.PhysicalCell0TimeStamp,a.SampicTimeStampA,a.SampicTimeStampB,
            a.FPGATimeStamp,mask};
}
} // namespace sampic::wire
