#include "residual_compression/codec_factory.h"
#include <stdexcept>
#include <string>

namespace waveform::compression {
std::unique_ptr<ResidualCodec> CodecFactory::create(std::string_view type,std::uint8_t rice_k) {
    if(type=="cpu-rice-golomb") return std::make_unique<CpuRiceGolombCodec>(rice_k);
    throw std::invalid_argument("unknown residual codec: "+std::string(type));
}
} // namespace waveform::compression
