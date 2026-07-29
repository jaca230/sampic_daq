#ifndef FRONTEND_COLLECTOR_MODE_EXTERNAL_TRIGGER_H
#define FRONTEND_COLLECTOR_MODE_EXTERNAL_TRIGGER_H

#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_config.h"

/// Builds one FrontendEvent per decoded external trigger using calibrated
/// timestamp windows; it deliberately does not fall back to hit clustering.
class FrontendCollectorModeExternalTrigger : public FrontendCollectorMode {
public:
    FrontendCollectorModeExternalTrigger(
        FrontendCollectorModeContext& context,
        FrontendCollectorModeExternalTriggerConfig config);
    bool collect() override;

private:
    FrontendCollectorModeExternalTriggerConfig mode_cfg_;
    std::chrono::steady_clock::time_point last_timestamp_{};
    std::chrono::milliseconds wait_timeout_{1000};
};

#endif
