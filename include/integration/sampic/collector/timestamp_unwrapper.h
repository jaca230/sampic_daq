#ifndef SAMPIC_TIMESTAMP_UNWRAPPER_H
#define SAMPIC_TIMESTAMP_UNWRAPPER_H
#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>

// Extend vendor modulo timestamps around a monotonic high-water mark, not the
// last-arriving hit: late FEB packets may legitimately belong to the prior epoch.
class SampicTimestampUnwrapper {
public:
    using Clock = std::chrono::steady_clock;
    double unwrap(double raw_ns, double modulus_ns, Clock::time_point arrival) {
        if (!std::isfinite(raw_ns) || !std::isfinite(modulus_ns) || modulus_ns < 0)
            throw std::runtime_error("invalid timestamp or rollover modulus");
        if (modulus_ && *modulus_ != modulus_ns)
            throw std::runtime_error("SAMPIC timestamp timebase changed within a run");
        modulus_ = modulus_ns;
        // Zero explicitly means a synthetic/unbounded timeline, e.g. simulator.
        if (modulus_ns == 0) return raw_ns;
        if (last_arrival_ && arrival > *last_arrival_ &&
            std::chrono::duration<long double, std::nano>(arrival - *last_arrival_).count() >= modulus_ns / 2)
            throw std::runtime_error("timestamp rollover epoch ambiguous after half a counter period without hits");
        if (!last_arrival_ || arrival > *last_arrival_) last_arrival_ = arrival;
        long double value = raw_ns;
        if (high_water_) {
            const long double epochs = std::round((*high_water_ - value) / modulus_ns);
            value += epochs * modulus_ns;
            if (std::abs(value - *high_water_) >= static_cast<long double>(modulus_ns) / 2)
                throw std::runtime_error("ambiguous half-period SAMPIC timestamp difference");
        }
        if (!high_water_ || value > *high_water_) high_water_ = value;
        const double result = static_cast<double>(value);
        if (!std::isfinite(result)) throw std::runtime_error("extended SAMPIC timestamp overflow");
        return result;
    }
private:
    std::optional<double> modulus_;
    std::optional<long double> high_water_;
    std::optional<Clock::time_point> last_arrival_;
};
#endif
