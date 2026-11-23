#pragma once

#include <cstdint>
#include <vector>

namespace sampic::event_builder {

struct Hit {
    std::uint16_t crate = 0;
    std::uint16_t board = 0;
    std::uint16_t channel = 0;
    std::uint64_t timestamp_ns = 0;
    std::vector<std::int16_t> waveform;
    // Simulation-only provenance; real MIDAS adapters leave this false.
    bool simulated_physics = false;
};

} // namespace sampic::event_builder
