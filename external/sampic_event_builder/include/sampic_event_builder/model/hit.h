#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <cmath>
#include <SAMPIC_256Ch_Type.h>
#include "processing/sampic_processing/collector/banks/sampic_hit_format.h"

namespace sampic::event_builder {

struct CompactHit {
    wire::CompactData data{};
    wire::CompactAdvanced advanced{};
};

struct Hit {
    std::uint16_t crate = 0;
    std::uint16_t board = 0;
    std::uint16_t channel = 0;
    std::uint64_t timestamp_ns = 0;
    std::vector<std::int16_t> waveform;
    // Simulation-only provenance; real MIDAS adapters leave this false.
    bool simulated_physics = false;
    // Aliases an owned received batch; no per-hit allocation or payload copy.
    // timestamp_ns above is only a coarse integer grouping key; the vendor's
    // fractional FirstCellTimeStamp is retained here and in the output.
    std::shared_ptr<const HitStruct> raw;
    std::shared_ptr<const CompactHit> compact; // Persisted-bank decoder provenance.

    std::size_t sample_count() const { return raw ? static_cast<std::size_t>(raw->DataSize) : waveform.size(); }
    // Cuts inspect vendor-corrected samples directly in compact-output units,
    // retaining fractional values. Nonfinite values follow SD's canonical zero.
    double corrected_sample(std::size_t index) const {
        if (!raw) return waveform[index];
        const double value=static_cast<double>(raw->CorrectedDataSamples[index])*(raw->ADCCorrected ? 10000.0 : 1.0);
        return std::isfinite(value) ? value : 0.0;
    }
};

} // namespace sampic::event_builder
