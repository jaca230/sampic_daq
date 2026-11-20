#include "sampic_event_builder/serialization/product_bank_encoder.h"
#include <cstring>
#include <stdexcept>

namespace sampic::event_builder {
namespace {
template<class T> void append(std::vector<std::uint8_t>& out, const T& value) {
    const auto old=out.size(); out.resize(old+sizeof(T)); std::memcpy(out.data()+old,&value,sizeof(T));
}
}

std::vector<std::uint8_t> ProductBankEncoder::encode(std::span<const ProcessedWaveform> waveforms) const
{
    if (waveforms.size() > UINT32_MAX) throw std::length_error("too many waveforms for bank");
    std::vector<std::uint8_t> output; append(output, products::ProductBankHeader{
        products::kProductBankMagic,products::kProductBankVersion,0,static_cast<std::uint32_t>(waveforms.size())});
    for(const auto& w:waveforms) {
        if(w.payload.size()>UINT32_MAX) throw std::length_error("waveform payload too large");
        append(output, products::ProductWaveformHeader{w.crate,w.board,w.channel,w.flags,w.fitter_id,w.codec_id,
            w.timestamp_ns,w.template_id,w.sample_count,w.baseline,w.amplitude_q,
            w.shift_q,w.chi_squared,w.degrees_of_freedom,0,
            static_cast<std::uint32_t>(w.payload.size()),w.payload_bit_count});
        output.insert(output.end(),w.payload.begin(),w.payload.end());
    }
    return output;
}

} // namespace sampic::event_builder
