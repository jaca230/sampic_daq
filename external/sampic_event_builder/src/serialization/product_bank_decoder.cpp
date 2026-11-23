#include "sampic_event_builder/serialization/product_bank_decoder.h"
#include "sampic_event_builder/products/event_product_format.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace sampic::event_builder {
static_assert(std::endian::native == std::endian::little);
namespace {
template<class T> T read(std::span<const std::uint8_t> bank, std::size_t& offset) {
    if (bank.size() - offset < sizeof(T)) throw std::runtime_error("truncated product bank");
    T value{}; std::memcpy(&value, bank.data() + offset, sizeof(T));
    offset += sizeof(T); return value;
}
}
Event ProductBankDecoder::decode(std::span<const std::uint8_t> bank) const {
    std::size_t offset = 0;
    const auto header = read<products::ProductBankHeader>(bank, offset);
    if (header.magic != products::kProductBankMagic || header.version != products::kProductBankVersion)
        throw std::runtime_error("unsupported product bank magic or version");
    if (header.reserved != 0 || header.waveform_count > (bank.size()-offset)/sizeof(products::ProductWaveformHeader))
        throw std::runtime_error("invalid product bank header");
    Event event{header.start_ns, header.end_ns, {}};
    event.hits.reserve(header.waveform_count);
    for (std::uint32_t i = 0; i < header.waveform_count; ++i) {
        const auto h = read<products::ProductWaveformHeader>(bank, offset);
        if (h.sample_count > (bank.size()-offset)/sizeof(std::int16_t))
            throw std::runtime_error("truncated waveform samples");
        Hit hit{h.crate, h.board, h.channel, h.timestamp_ns, {}, false};
        hit.waveform.resize(h.sample_count);
        if (h.sample_count) std::memcpy(hit.waveform.data(), bank.data()+offset, h.sample_count*sizeof(std::int16_t));
        offset += h.sample_count*sizeof(std::int16_t);
        event.hits.push_back(std::move(hit));
    }
    if (offset != bank.size()) throw std::runtime_error("trailing product bank bytes");
    return event;
}
}
