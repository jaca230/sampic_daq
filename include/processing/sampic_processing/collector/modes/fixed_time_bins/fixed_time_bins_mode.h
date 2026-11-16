#ifndef FRONTEND_COLLECTOR_FIXED_TIME_BINS_MODE_H
#define FRONTEND_COLLECTOR_FIXED_TIME_BINS_MODE_H
#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/fixed_time_bins/fixed_time_bins_config.h"
#include "integration/sampic/collector/timestamp_unwrapper.h"
#include <map>
#include <limits>
#include <optional>

class FrontendCollectorModeFixedTimeBins : public FrontendCollectorMode {
public:
    FrontendCollectorModeFixedTimeBins(FrontendCollectorModeContext& context,
                                     FrontendCollectorModeFixedTimeBinsConfig config);
    bool collect() override;
    bool flush() override;
    bool hasPendingOutput() const override {
        return next_bin_ && last_bin_ && *next_bin_ <= *last_bin_;
    }
    static void validate(const FrontendCollectorModeFixedTimeBinsConfig& config);
private:
    struct Bin {
        std::vector<std::shared_ptr<SampicEvent>> parents;
        std::vector<const HitStruct*> hits;
    };
    using Clock = std::chrono::steady_clock;
    FrontendCollectorModeFixedTimeBinsConfig config_;
    SampicTimestampUnwrapper timestamp_unwrapper_;
    std::map<std::int64_t, Bin> bins_;
    std::optional<std::int64_t> next_bin_, ready_through_, last_bin_;
    bool emitted_ = false;
    double max_timestamp_ns_ = -std::numeric_limits<double>::infinity();
    Clock::time_point last_input_{};
    Clock::time_point last_timestamp_ = Clock::time_point::min();
    std::int64_t indexFor(double timestamp_ns) const;
    std::pair<double, double> bounds(std::int64_t index) const;
    void advanceReady(std::int64_t index);
    bool emitReady(std::size_t limit);
};
#endif
