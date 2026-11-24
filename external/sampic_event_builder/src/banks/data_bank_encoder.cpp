#include "sampic_event_builder/banks/data_bank.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace sampic::event_builder::banks {
static_assert(std::endian::native == std::endian::little);
namespace {
template<class T> void append(std::vector<std::uint8_t>& out, const T& value) {
    const auto old = out.size(); out.resize(old + sizeof(T));
    std::memcpy(out.data() + old, &value, sizeof(T));
}
}
std::vector<std::uint8_t> DataBank::encode(const Event& event) const {
    if (event.hits.size() > UINT32_MAX) throw std::length_error("too many waveforms");
    std::vector<std::uint8_t> output;
    append(output, DataBank::Header{DataBank::kProductBankMagic,
        DataBank::kProductBankVersion, 0, static_cast<std::uint32_t>(event.hits.size()),
        event.start_ns, event.end_ns});
    for (const auto& hit : event.hits) {
        DataBank::Record record{};
        record.crate=hit.crate;
        if (hit.raw) {
            record.data=wire::compact_data(*hit.raw);
            record.advanced=wire::compact_advanced(*hit.raw);
        } else if (hit.compact) {
            record.data=hit.compact->data;
            record.advanced=hit.compact->advanced;
        } else {
            // Software-only benchmark/simulator hits have no vendor structure.
            if (hit.waveform.size()>64 || hit.board>3 || hit.channel>63)
                throw std::invalid_argument("invalid simulated waveform");
            auto& data=record.data;
            data.feb_index=hit.board; data.channel=hit.channel;
            data.sampic_index=hit.channel/16; data.sampic_channel_index=hit.channel%16;
            data.waveform_size=hit.waveform.size();
            data.first_cell_timestamp_ns=static_cast<double>(hit.timestamp_ns);
            std::copy(hit.waveform.begin(),hit.waveform.end(),data.waveform);
        }
        append(output,record);
    }
    return output;
}
}
