#pragma once
#include <cstdint>
#include <string>
namespace sampic::event_builder::selection {
struct Summary {
    std::string id;
    bool enabled=false;
    std::int32_t priority=0;
    std::uint64_t evaluated_events=0, rejected_events=0, evaluated_hits=0, rejected_hits=0;
};
inline bool valid_id(const std::string& id) {
    if (id.empty() || id.size()>31) return false;
    for (const char c:id) if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return false;
    return true;
}
}
