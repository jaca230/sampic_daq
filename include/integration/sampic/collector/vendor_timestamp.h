#ifndef SAMPIC_VENDOR_TIMESTAMP_H
#define SAMPIC_VENDOR_TIMESTAMP_H
#include <cmath>
#include <stdexcept>

// DecodeEvent uses a float sampling period in ps and a 40-bit FPGA cycle counter.
// Match its arithmetic, including float rounding at non-power-of-two frequencies.
inline double sampicTimestampModulusNs(int sampling_frequency_mhz) {
    if (sampling_frequency_mhz <= 0) throw std::invalid_argument("invalid SAMPIC sampling frequency");
    const float sampling_period_ps = 1000000.0 / static_cast<float>(sampling_frequency_mhz);
    return std::ldexp(64.0 * (sampling_period_ps / 1000.0), 40);
}
#endif
