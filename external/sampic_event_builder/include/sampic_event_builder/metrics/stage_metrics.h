#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sampic::event_builder {

struct StageMeasurement {
    std::string name;
    std::uint64_t items = 0;
    std::uint64_t bytes = 0;
    std::chrono::nanoseconds elapsed{0};
    [[nodiscard]] double items_per_second() const;
    [[nodiscard]] double megabytes_per_second() const;
};

class StageMetrics {
public:
    void add(std::string_view name, std::uint64_t items, std::uint64_t bytes,
             std::chrono::nanoseconds elapsed);
    [[nodiscard]] const std::vector<StageMeasurement>& measurements() const { return measurements_; }
private:
    std::vector<StageMeasurement> measurements_;
};

} // namespace sampic::event_builder
