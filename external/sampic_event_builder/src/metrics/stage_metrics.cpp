#include "sampic_event_builder/metrics/stage_metrics.h"

namespace sampic::event_builder {

double StageMeasurement::items_per_second() const
{
    return elapsed.count() == 0 ? 0.0 : static_cast<double>(items) * 1.0e9 / elapsed.count();
}
double StageMeasurement::megabytes_per_second() const
{
    return elapsed.count() == 0 ? 0.0 : static_cast<double>(bytes) * 1.0e3 / elapsed.count();
}
void StageMetrics::add(std::string_view name, std::uint64_t items, std::uint64_t bytes,
                       std::chrono::nanoseconds elapsed)
{
    measurements_.push_back({std::string(name), items, bytes, elapsed});
}

} // namespace sampic::event_builder
