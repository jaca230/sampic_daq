#include "sampic_event_builder/banks/data_bank.h"
#include <bit>
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace sampic::event_builder::banks {
static_assert(std::endian::native == std::endian::little);
namespace {
template<class T> T read(std::span<const std::uint8_t> bank, std::size_t& offset) {
    if (bank.size() - offset < sizeof(T)) throw std::runtime_error("truncated product bank");
    T value{}; std::memcpy(&value, bank.data() + offset, sizeof(T));
    offset += sizeof(T); return value;
}
}
Event DataBank::decode(std::span<const std::uint8_t> bank) const {
    std::size_t offset = 0;
    const auto header = read<DataBank::Header>(bank, offset);
    if (header.magic != DataBank::kProductBankMagic || header.version != DataBank::kProductBankVersion)
        throw std::runtime_error("unsupported product bank magic or version");
    if (header.reserved != 0 || header.start_ns > header.end_ns ||
        (bank.size()-offset)%sizeof(DataBank::Record) != 0 ||
        header.waveform_count != (bank.size()-offset)/sizeof(DataBank::Record))
        throw std::runtime_error("invalid product bank header");
    Event event{header.start_ns, header.end_ns, {}};
    event.hits.reserve(header.waveform_count);
    for (std::uint32_t i = 0; i < header.waveform_count; ++i) {
        const auto record=read<DataBank::Record>(bank,offset);
        const auto& d=record.data;
        if (d.waveform_size>64 || d.feb_index>3 || d.channel>63 ||
            d.sampic_index>3 || d.sampic_channel_index>15 || d.cell_info>63 ||
            d.first_cell_physical_index>63 || !std::isfinite(d.first_cell_timestamp_ns))
            throw std::runtime_error("invalid compact product hit");
        Hit hit;
        hit.crate=record.crate; hit.board=d.feb_index; hit.channel=d.channel;
        const double timestamp=d.first_cell_timestamp_ns;
        hit.timestamp_ns=timestamp<=0 ? 0 :
            timestamp>=static_cast<double>(std::numeric_limits<std::uint64_t>::max()) ?
            std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(timestamp);
        hit.waveform.assign(d.waveform,d.waveform+d.waveform_size);
        hit.compact=std::make_shared<CompactHit>(CompactHit{d,record.advanced});
        event.hits.push_back(std::move(hit));
    }
    if (offset != bank.size()) throw std::runtime_error("trailing product bank bytes");
    return event;
}
}
