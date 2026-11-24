#pragma once
#include "sampic_event_builder/model/selection_summary.h"
#include <cstring>
#include <stdexcept>
#include <vector>
namespace sampic::event_builder::banks {
class SelectionSummaryBank {
public:
    static constexpr const char* name="BS00";
#pragma pack(push,1)
    struct Header {
        std::uint32_t magic=0x31535342; // BSS1
        std::uint16_t version=1, record_size=72;
        std::uint64_t cycle_id=0;
        std::uint32_t cut_count=0, reserved=0;
    };
    struct Record {
        char id[32]{};
        std::uint32_t enabled=0;
        std::int32_t priority=0;
        std::uint64_t evaluated_events=0, rejected_events=0, evaluated_hits=0, rejected_hits=0;
    };
#pragma pack(pop)

    static std::vector<std::uint8_t> encode(std::uint64_t cycle,const std::vector<selection::Summary>& cuts) {
        if (cuts.size()>UINT32_MAX) throw std::length_error("too many cuts");
        Header h; h.cycle_id=cycle; h.cut_count=cuts.size();
        std::vector<std::uint8_t> bytes(sizeof(h)+cuts.size()*sizeof(Record));
        std::memcpy(bytes.data(),&h,sizeof(h));
        for (std::size_t i=0;i<cuts.size();++i) {
            const auto& c=cuts[i];
            if (!selection::valid_id(c.id)) throw std::invalid_argument("invalid cut ID");
            Record record;
            std::memcpy(record.id,c.id.data(),c.id.size()); record.enabled=c.enabled; record.priority=c.priority;
            record.evaluated_events=c.evaluated_events; record.rejected_events=c.rejected_events;
            record.evaluated_hits=c.evaluated_hits; record.rejected_hits=c.rejected_hits;
            std::memcpy(bytes.data()+sizeof(h)+i*sizeof(record),&record,sizeof(record));
        }
        return bytes;
    }
};
static_assert(sizeof(SelectionSummaryBank::Header)==24 && sizeof(SelectionSummaryBank::Record)==72);
}
