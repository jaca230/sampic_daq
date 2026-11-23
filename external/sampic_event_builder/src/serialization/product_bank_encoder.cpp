#include "sampic_event_builder/serialization/product_bank_encoder.h"
#include "sampic_event_builder/products/event_product_format.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace sampic::event_builder {
static_assert(std::endian::native == std::endian::little);
namespace {
template<class T> void append(std::vector<std::uint8_t>& out, const T& value) {
    const auto old = out.size(); out.resize(old + sizeof(T));
    std::memcpy(out.data() + old, &value, sizeof(T));
}
}
std::vector<std::uint8_t> ProductBankEncoder::encode(const Event& event) const {
    if (event.hits.size() > UINT32_MAX) throw std::length_error("too many waveforms");
    std::vector<std::uint8_t> output;
    append(output, products::ProductBankHeader{products::kProductBankMagic,
        products::kProductBankVersion, 0, static_cast<std::uint32_t>(event.hits.size()),
        event.start_ns, event.end_ns});
    for (const auto& hit : event.hits) {
        if (hit.waveform.size() > UINT32_MAX) throw std::length_error("too many samples");
        append(output, products::ProductWaveformHeader{hit.crate, hit.board, hit.channel,
            hit.timestamp_ns, static_cast<std::uint32_t>(hit.waveform.size())});
        const auto offset = output.size();
        const auto bytes = hit.waveform.size() * sizeof(std::int16_t);
        output.resize(offset + bytes);
        if (bytes) std::memcpy(output.data() + offset, hit.waveform.data(), bytes);
    }
    return output;
}
}
