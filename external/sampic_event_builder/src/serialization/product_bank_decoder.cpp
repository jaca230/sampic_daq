#include "sampic_event_builder/serialization/product_bank_decoder.h"
#include "sampic_event_builder/products/event_product_format.h"

#include <cstring>
#include <stdexcept>

namespace sampic::event_builder {
namespace {
template<class T> T read(std::span<const std::uint8_t> bank, std::size_t& offset) {
    if (bank.size()-offset < sizeof(T)) throw std::runtime_error("truncated product bank header");
    T value{}; std::memcpy(&value,bank.data()+offset,sizeof(T)); offset+=sizeof(T); return value;
}
}

std::vector<ProcessedWaveform> ProductBankDecoder::decode(std::span<const std::uint8_t> bank) const
{
    std::size_t offset=0;
    const auto header=read<products::ProductBankHeader>(bank,offset);
    if(header.magic!=products::kProductBankMagic || header.version!=products::kProductBankVersion)
        throw std::runtime_error("unsupported product bank magic or version");
    std::vector<ProcessedWaveform> result; result.reserve(header.waveform_count);
    for(std::uint32_t i=0;i<header.waveform_count;++i) {
        const auto h=read<products::ProductWaveformHeader>(bank,offset);
        if(bank.size()-offset<h.payload_size) throw std::runtime_error("truncated waveform payload");
        ProcessedWaveform w;
        w.crate=h.crate; w.board=h.board; w.channel=h.channel; w.flags=h.flags;
        w.fitter_id=h.fitter_id; w.codec_id=h.codec_id; w.timestamp_ns=h.timestamp_ns;
        w.template_id=h.template_id; w.sample_count=h.sample_count; w.baseline=h.baseline;
        w.amplitude_q=h.amplitude_q; w.shift_q=h.shift_q;
        w.chi_squared=h.chi_squared; w.degrees_of_freedom=h.degrees_of_freedom;
        w.payload_bit_count=h.payload_bit_count;
        w.payload.assign(bank.begin()+offset,bank.begin()+offset+h.payload_size); offset+=h.payload_size;
        result.push_back(std::move(w));
    }
    if(offset!=bank.size()) throw std::runtime_error("trailing bytes in product bank");
    return result;
}

} // namespace sampic::event_builder
