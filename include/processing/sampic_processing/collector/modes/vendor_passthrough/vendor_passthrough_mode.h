#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_MODE_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_PASSTHROUGH_MODE_H

#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/vendor_passthrough/vendor_passthrough_config.h"

#include <chrono>

/// Maps each decoded vendor EventStruct to one FrontendEvent without grouping
/// or splitting its hits by timestamp.
class FrontendCollectorModeVendorPassthrough : public FrontendCollectorMode {
public:
    FrontendCollectorModeVendorPassthrough(
        FrontendCollectorModeContext& context,
        FrontendCollectorModeVendorPassthroughConfig config);

    bool collect() override;

private:
    FrontendCollectorModeVendorPassthroughConfig mode_cfg_;
    std::chrono::milliseconds wait_timeout_;
    std::chrono::steady_clock::time_point last_timestamp_{
        std::chrono::steady_clock::time_point::min()};
};

#endif
