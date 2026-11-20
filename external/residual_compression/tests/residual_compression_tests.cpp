#include "residual_compression/cpu_rice_golomb_codec.h"
#include <cassert>
#include <vector>

int main() {
    waveform::compression::CpuRiceGolombCodec codec(4);
    const std::vector<std::int32_t> values{0, 1, -1, 4, -8, 31, -77, 2};
    assert(codec.decode(codec.encode(values)) == values);
}
