#pragma once
#include "sampic_event_builder/pipeline/pipeline_event.h"
#include "sampic_event_builder/banks/data_bank.h"
#include <midas.h>
#include <cstring>
#include <stdexcept>
#include <string>

namespace sampic::event_builder {
// limit includes EVENT_HEADER, BANK_HEADER, BANK32 headers and 8-byte padding.
// Validate every bank before touching the destination (also used during EOR).
inline INT write_midas_banks(char* destination, const FinalizedEvent& event,
                            std::size_t limit) {
    constexpr auto framing = sizeof(EVENT_HEADER) + sizeof(BANK_HEADER);
    if (limit < framing)
        throw std::overflow_error("MIDAS output limit is smaller than event framing; data incomplete");
    auto remaining = limit - framing;
    const auto check = [&](const std::string& name, const std::vector<std::uint8_t>& bytes) {
        if (name.size() != 4) throw std::invalid_argument("MIDAS bank name must contain four characters");
        const auto padding = (8 - bytes.size() % 8) % 8;
        const auto overhead = sizeof(BANK32) + padding;
        if (overhead > remaining || bytes.size() > remaining - overhead)
            throw std::overflow_error("MIDAS output event exceeds " + std::to_string(limit) +
                "-byte limit (bank " + name + ", payload=" + std::to_string(bytes.size()) +
                "); data incomplete");
        remaining -= overhead + bytes.size();
    };
    if (!event.product_bank.empty()) check(banks::DataBank::name, event.product_bank);
    for (const auto& bank : event.diagnostic_banks) check(bank.name, bank.payload);

    bk_init32(destination);
    const auto add = [&](const char* name, const std::vector<std::uint8_t>& bytes) {
        std::uint8_t* data = nullptr;
        bk_create(destination, name, TID_UINT8, reinterpret_cast<void**>(&data));
        if (!bytes.empty()) std::memcpy(data, bytes.data(), bytes.size());
        bk_close(destination, data + bytes.size());
    };
    if (!event.product_bank.empty()) add(banks::DataBank::name, event.product_bank);
    for (const auto& bank : event.diagnostic_banks) add(bank.name.c_str(), bank.payload);
    return bk_size(destination);
}
} // namespace sampic::event_builder
