#ifndef FRONTEND_COLLECTOR_MODE_H
#define FRONTEND_COLLECTOR_MODE_H

#include <memory>
#include <vector>

#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "integration/sampic/collector/sampic_event_buffer.h"  // for TimestampedSampicEvent

/// Abstract base class for frontend collector modes.
/// A mode defines *how* data is gathered and converted into FrontendEvents.
/// The collector will invoke collect() periodically according to configuration.
class FrontendCollectorMode {
public:
    FrontendCollectorMode(FrontendEventBuffer& buffer,
                          const FrontendEventCollectorConfig& cfg)
        : buffer_(buffer), cfg_(cfg) {}

    virtual ~FrontendCollectorMode() = default;

    /// Perform one data collection cycle.
    /// The input is a vector of new SAMPIC events fetched by the collector.
    /// Implementations may push zero or more FrontendEvents into the buffer.
    ///
    /// Returns true if succeeded with no errors. (may replace with some status code enum in the future)
    virtual bool collect(const std::vector<TimestampedSampicEvent>& new_events) = 0;

protected:
    FrontendEventBuffer& buffer_;
    const FrontendEventCollectorConfig& cfg_;
};

#endif // FRONTEND_COLLECTOR_MODE_H
