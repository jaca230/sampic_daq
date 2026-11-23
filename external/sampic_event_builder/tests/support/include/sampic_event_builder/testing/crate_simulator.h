#pragma once

#include "sampic_event_builder/model/hit.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace sampic::event_builder {

struct CrateSimulatorConfig {
    std::uint16_t crate = 0;
    std::uint32_t channels = 256;
    std::uint32_t samples = 64;
    double hit_rate_hz = 100'000.0;
    double physics_fraction = 0.10;
    std::uint64_t seed = 1;
};

class CrateSimulator {
public:
    explicit CrateSimulator(CrateSimulatorConfig config);
    std::vector<Hit> generate(std::size_t hit_count);

private:
    Hit make_hit(bool physics);
    CrateSimulatorConfig config_;
    std::mt19937_64 random_;
    std::uint64_t timestamp_ns_ = 0;
    std::uint32_t physics_hits_remaining_ = 0;
};

} // namespace sampic::event_builder
