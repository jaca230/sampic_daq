#pragma once
#include "sampic_event_builder/model/timing_phases.h"
#include <cstring>
#include <vector>
#include <span>
#include <stdexcept>
namespace sampic::event_builder::banks {
class AcquisitionTimingBank {
public:
    static constexpr const char* name = "SA";
#pragma pack(push,1)
    struct Record {
        char magic[4] = {'S','A','T','1'};
        std::uint16_t version = 1, record_size = 72;
        std::uint64_t acquisition_id = 0;
        // Source-host steady clock at acquisition completion; not hardware time.
        std::uint64_t readout_end_ns = 0;
        std::uint64_t hit_count = 0;
        timing::Phases duration;
        std::uint64_t retries = 0;
    };
#pragma pack(pop)
    static Record decode(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != sizeof(Record)) throw std::runtime_error("invalid SA bank length");
        Record record;
        std::memcpy(&record, bytes.data(), sizeof(record));
        if (std::memcmp(record.magic, "SAT1", 4) || record.version != 1 || record.record_size != sizeof(record))
            throw std::runtime_error("unsupported SA bank format");
        return record;
    }
    static std::vector<std::uint8_t> encode(const Record& record) {
        std::vector<std::uint8_t> bytes(sizeof(record));
        std::memcpy(bytes.data(), &record, sizeof(record));
        return bytes;
    }
};
static_assert(sizeof(AcquisitionTimingBank::Record)==72);
}
namespace sampic::event_builder::timing {
using Acquisition = banks::AcquisitionTimingBank::Record;
}
