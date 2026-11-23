#pragma once

#include "sampic_event_builder/model/hit.h"

#include <cstdint>
#include <vector>

namespace sampic::event_builder {

struct Event {
    std::uint64_t start_ns = 0;
    std::uint64_t end_ns = 0;
    std::vector<Hit> hits;
};

} // namespace sampic::event_builder
