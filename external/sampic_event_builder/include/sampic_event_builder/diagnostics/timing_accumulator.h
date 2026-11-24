#pragma once
#include "sampic_event_builder/banks/acquisition_timing_bank.h"
#include "sampic_event_builder/banks/acquisition_summary_bank.h"
#include "sampic_event_builder/banks/collection_timing_bank.h"
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace sampic::event_builder::timing {
class Accumulator {
public:
    void add(std::uint16_t crate, const Acquisition& acquisition);
    std::map<std::uint16_t, Summary> take(std::uint64_t cycle_id);
    void reset() { states_.clear(); reads_ = 0; }
    std::uint64_t reads() const { return reads_; }
private:
    struct State { std::optional<std::uint64_t> last_id; Summary summary; };
    std::map<std::uint16_t, State> states_;
    std::uint64_t reads_ = 0;
};
} // namespace sampic::event_builder::timing
