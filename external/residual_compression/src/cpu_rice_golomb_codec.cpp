#include "residual_compression/cpu_rice_golomb_codec.h"

#include <stdexcept>

namespace waveform::compression {
namespace {
class Writer {
public:
    void bit(bool value) { if (count_ % 8 == 0) bytes_.push_back(0); if (value) bytes_.back() |= 1u << (count_ % 8); ++count_; }
    void bits(std::uint32_t value, std::uint8_t count) { for (std::uint8_t i = 0; i < count; ++i) bit((value >> i) & 1u); }
    std::uint32_t count() const { return count_; }
    std::vector<std::uint8_t> take() { return std::move(bytes_); }
private: std::vector<std::uint8_t> bytes_; std::uint32_t count_ = 0;
};
class Reader {
public:
    Reader(const EncodedBlock& block) : block_(block) {}
    bool bit() { if (position_ >= block_.bit_count) throw std::runtime_error("truncated Rice block"); const bool v=(block_.bytes[position_/8]>>(position_%8))&1u; ++position_; return v; }
    std::uint32_t bits(std::uint8_t count) { std::uint32_t v=0; for(std::uint8_t i=0;i<count;++i) if(bit()) v|=1u<<i; return v; }
private: const EncodedBlock& block_; std::uint32_t position_=0;
};
std::uint32_t zigzag(std::int32_t value) { return (static_cast<std::uint32_t>(value) << 1) ^ static_cast<std::uint32_t>(value >> 31); }
std::int32_t unzigzag(std::uint32_t value) { return static_cast<std::int32_t>((value >> 1) ^ (0u - (value & 1u))); }
} // namespace

CpuRiceGolombCodec::CpuRiceGolombCodec(std::uint8_t rice_k) : rice_k_(rice_k) {
    if (rice_k_ > 20) throw std::invalid_argument("Rice k must be at most 20");
}
EncodedBlock CpuRiceGolombCodec::encode(std::span<const std::int32_t> values) const {
    Writer writer;
    for (const auto value : values) {
        const auto code=zigzag(value), quotient=code>>rice_k_;
        for(std::uint32_t i=0;i<quotient;++i) writer.bit(true);
        writer.bit(false); writer.bits(code & ((1u<<rice_k_)-1u), rice_k_);
    }
    EncodedBlock block{kCodecId, static_cast<std::uint32_t>(values.size()), writer.count(), writer.take()};
    // The decoding parameter is part of the payload, not out-of-band configuration.
    block.bytes.insert(block.bytes.begin(), rice_k_);
    block.bit_count += 8;
    return block;
}
std::vector<std::int32_t> CpuRiceGolombCodec::decode(const EncodedBlock& input) const {
    if (input.codec_id != kCodecId || input.bytes.empty()) throw std::invalid_argument("not a Rice-Golomb block");
    const auto k=input.bytes.front();
    EncodedBlock block=input; block.bytes.erase(block.bytes.begin()); block.bit_count-=8;
    Reader reader(block); std::vector<std::int32_t> values; values.reserve(block.value_count);
    for(std::uint32_t n=0;n<block.value_count;++n) { std::uint32_t q=0; while(reader.bit()) ++q; values.push_back(unzigzag((q<<k)|reader.bits(k))); }
    return values;
}

} // namespace waveform::compression
