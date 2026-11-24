#pragma once
#include "sampic_event_builder/model/timing_phases.h"
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
namespace sampic::event_builder::banks {
class AcquisitionSummaryBank {
public:
    static std::string name(std::uint16_t crate) {
        if (crate > 99) throw std::invalid_argument("BT bank crate must be in 0..99");
        return "BT" + std::string(crate < 10 ? "0" : "") + std::to_string(crate);
    }
#pragma pack(push,1)
    struct Record {
        char magic[4] = {'B','T','S','1'};
        std::uint16_t version = 1, record_size = 128;
        std::uint64_t cycle_id = 0, read_count = 0, hit_count = 0;
        timing::Phases sum, maximum;
        std::uint64_t retries_sum = 0, retries_max = 0;
        std::uint64_t first_acquisition_id = 0, last_acquisition_id = 0;
    };
#pragma pack(pop)
    static std::vector<std::uint8_t> encode(const Record& record) {
        std::vector<std::uint8_t> bytes(sizeof(record));
        std::memcpy(bytes.data(), &record, sizeof(record));
        return bytes;
    }
};
static_assert(sizeof(AcquisitionSummaryBank::Record)==128);
}
namespace sampic::event_builder::timing {
using Summary = banks::AcquisitionSummaryBank::Record;
}
