#include "sampic_event_builder/testing/crate_simulator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sampic::event_builder {

CrateSimulator::CrateSimulator(CrateSimulatorConfig config)
    : config_(config), random_(config.seed)
{
    if (config_.channels == 0 || config_.samples == 0 || config_.hit_rate_hz <= 0.0) {
        throw std::invalid_argument("simulator channels, samples, and hit rate must be positive");
    }
}

std::vector<Hit> CrateSimulator::generate(std::size_t hit_count)
{
    std::bernoulli_distribution physics(config_.physics_fraction);
    std::uniform_int_distribution<std::uint32_t> physics_multiplicity(4, 8);
    std::vector<Hit> hits;
    hits.reserve(hit_count);
    for (std::size_t index = 0; index < hit_count; ++index) {
        if (physics_hits_remaining_ == 0 && physics(random_)) {
            physics_hits_remaining_ = physics_multiplicity(random_);
        }
        const bool in_physics_group = physics_hits_remaining_ > 0;
        hits.push_back(make_hit(in_physics_group));
        if (in_physics_group) --physics_hits_remaining_;
    }
    return hits;
}

Hit CrateSimulator::make_hit(bool physics)
{
    std::uniform_int_distribution<std::uint32_t> channel(0, config_.channels - 1);
    std::normal_distribution<double> noise(0.0, 2.0);
    std::normal_distribution<double> amplitude(physics ? 420.0 : 110.0, physics ? 40.0 : 20.0);
    std::exponential_distribution<double> background_gap(config_.hit_rate_hz / 1.0e9);
    std::uniform_int_distribution<std::uint64_t> physics_gap(4, 25);

    timestamp_ns_ += physics ? physics_gap(random_) : std::max<std::uint64_t>(
        1, static_cast<std::uint64_t>(background_gap(random_)));
    Hit hit;
    const auto selected_channel = channel(random_);
    hit.crate = config_.crate;
    hit.board = static_cast<std::uint16_t>(selected_channel / 64);
    hit.channel = static_cast<std::uint16_t>(selected_channel);
    hit.timestamp_ns = timestamp_ns_;
    hit.simulated_physics = physics;
    hit.waveform.resize(config_.samples);

    const double scale = std::max(10.0, amplitude(random_));
    const double onset = 0.30 * static_cast<double>(config_.samples);
    for (std::size_t sample = 0; sample < hit.waveform.size(); ++sample) {
        const double t = static_cast<double>(sample) - onset;
        const double pulse = t > 0.0
            ? scale * (std::exp(-t / 18.0) - std::exp(-t / 2.5))
            : 0.0;
        hit.waveform[sample] = static_cast<std::int16_t>(std::lround(1000.0 + pulse + noise(random_)));
    }
    return hit;
}

} // namespace sampic::event_builder
