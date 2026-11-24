#pragma once
#include <cstdint>
namespace sampic::event_builder::timing {
#pragma pack(push,1)
struct Phases {
    std::uint64_t prepare_us = 0, read_us = 0, decode_us = 0, total_us = 0;
};
#pragma pack(pop)
}
