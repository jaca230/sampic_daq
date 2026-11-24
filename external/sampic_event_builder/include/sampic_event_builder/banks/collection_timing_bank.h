#pragma once
#include "sampic_event_builder/model/timing_phases.h"
#include <cstring>
#include <vector>
namespace sampic::event_builder::banks {
class CollectionTimingBank {
public:
    static constexpr const char* name = "BC00";
#pragma pack(push,1)
    struct Record {
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
    static std::vector<std::uint8_t> encode(const Record& record) {
        std::vector<std::uint8_t> bytes(sizeof(record));
        std::memcpy(bytes.data(), &record, sizeof(record));
        return bytes;
    }
};
static_assert(sizeof(CollectionTimingBank::Record)==128);
}
namespace sampic::event_builder::timing {
using CollectorCycle = banks::CollectionTimingBank::Record;
}
