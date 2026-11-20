#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_RAW_PASSTHROUGH_MODE_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_VENDOR_RAW_PASSTHROUGH_MODE_H

#include "processing/sampic_processing/collector/modes/frontend_collector_mode.h"
#include "processing/sampic_processing/collector/modes/vendor_raw_passthrough/vendor_raw_passthrough_config.h"

#include <chrono>

/// Preserves vendor-event boundaries while publishing ordered, uncorrected ADC
/// samples. This mode is intended for event-building and compression studies.
class FrontendCollectorModeVendorRawPassthrough final
    : public FrontendCollectorMode {
public:
    FrontendCollectorModeVendorRawPassthrough(
        FrontendCollectorModeContext& context,
        FrontendCollectorModeVendorRawPassthroughConfig config);

    bool collect() override;

private:
    FrontendCollectorModeVendorRawPassthroughConfig mode_cfg_;
    std::chrono::milliseconds wait_timeout_;
    std::chrono::steady_clock::time_point last_timestamp_{
        std::chrono::steady_clock::time_point::min()};
};

#endif
