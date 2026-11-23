#pragma once
#include <cstdint>

namespace sampic::event_builder::timing {
// Packed little-endian timing contracts, separate from waveform products.
#pragma pack(push, 1)
struct Phases {
    std::uint64_t prepare_us = 0, read_us = 0, decode_us = 0, total_us = 0;
};
struct Acquisition {
    char magic[4] = {'S','A','T','1'};
    std::uint16_t version = 1, record_size = 72;
    std::uint64_t acquisition_id = 0;
    // Source-host steady clock at acquisition completion; not hardware time.
    std::uint64_t readout_end_ns = 0;
    std::uint64_t hit_count = 0;
    Phases duration;
    std::uint64_t retries = 0;
};
struct Summary {
    char magic[4] = {'B','T','S','1'};
    std::uint16_t version = 1, record_size = 128;
    std::uint64_t cycle_id = 0, read_count = 0, hit_count = 0;
    Phases sum, maximum;
    std::uint64_t retries_sum = 0, retries_max = 0;
    std::uint64_t first_acquisition_id = 0, last_acquisition_id = 0;
};
struct CollectorCycle {
    char magic[4] = {'B','C','T','1'};
    std::uint16_t version = 1, record_size = 128;
    std::uint64_t cycle_id = 0;
    // Builder steady-clock ns relative to run start; not hardware time.
    std::uint64_t start_ns = 0;
    std::uint64_t input_events = 0, input_hits = 0;
    std::uint64_t output_events = 0, output_hits = 0;
    std::uint64_t selected_events = 0, rejected_events = 0;
    std::uint64_t input_read_ns = 0, group_build_ns = 0, cuts_ns = 0, finalize_ns = 0;
    std::uint64_t output_wait_ns = 0, total_ns = 0;
    std::uint64_t flags = 0; // bit 0: end-of-run flush cycle
};
#pragma pack(pop)
static_assert(sizeof(Acquisition) == 72);
static_assert(sizeof(Summary) == 128);
static_assert(sizeof(CollectorCycle) == 128);
} // namespace sampic::event_builder::timing
