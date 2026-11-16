#ifndef FRONTEND_PERIODIC_READOUT_MODE_H
#define FRONTEND_PERIODIC_READOUT_MODE_H
#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/periodic_readout/periodic_readout_config.h"
#include <deque>
#include <functional>
#include <optional>
class FrontendCollectorModePeriodicReadout : public FrontendCollectorMode {
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    FrontendCollectorModePeriodicReadout(FrontendCollectorModeContext& context,
        FrontendCollectorModePeriodicReadoutConfig config, Now now = Clock::now);
    void onStart() override;
    bool collect() override;
    bool flush() override;
    bool hasPendingOutput() const override { return !ready_.empty() || !hits_.empty(); }
    static void validate(const FrontendCollectorModePeriodicReadoutConfig& config);
private:
    FrontendCollectorModePeriodicReadoutConfig config_;
    Now now_;
    std::optional<Clock::time_point> zero_;
    Clock::time_point last_input_ = Clock::time_point::min();
    std::uint64_t next_tick_ = 0;
    bool flushed_ = false;
    std::vector<std::shared_ptr<SampicEvent>> parents_;
    std::vector<const HitStruct*> hits_;
    std::deque<std::shared_ptr<FrontendEvent>> ready_;
    void receive();
    void readout(bool final);
    void send();
};
#endif
