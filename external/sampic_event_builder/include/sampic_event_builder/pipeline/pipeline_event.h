#pragma once
#include "sampic_event_builder/model/event.h"
#include <string>
namespace sampic::event_builder {
struct OutputBank {
    std::string name;
    std::vector<std::uint8_t> payload;
};
struct FinalizedEvent {
    std::uint64_t start_ns = 0, end_ns = 0;
    std::vector<std::uint8_t> product_bank;
    std::vector<OutputBank> diagnostic_banks{};
};
} // namespace sampic::event_builder
