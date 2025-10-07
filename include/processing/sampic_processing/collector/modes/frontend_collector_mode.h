#ifndef FRONTEND_COLLECTOR_MODE_H
#define FRONTEND_COLLECTOR_MODE_H

#include <memory>
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "integration/sampic/collector/sampic_event_buffer.h"

/**
 * @brief Abstract base class for frontend collector modes.
 * Each mode defines how SAMPIC events are transformed into FrontendEvents.
 */
class FrontendCollectorMode {
public:
    FrontendCollectorMode(SampicEventBuffer& sampic_buffer,
                          FrontendEventBuffer& frontend_buffer,
                          const FrontendEventCollectorConfig& cfg)
        : sampic_buffer_(sampic_buffer),
          frontend_buffer_(frontend_buffer),
          cfg_(cfg) {}

    virtual ~FrontendCollectorMode() = default;

    /**
     * @brief Perform one data collection cycle.
     * The mode may fetch new SampicEvents from sampic_buffer_ and push
     * one or more FrontendEvents into frontend_buffer_.
     * @return true if succeeded (no fatal error).
     */
    virtual bool collect() = 0;

protected:
    SampicEventBuffer& sampic_buffer_;
    FrontendEventBuffer& frontend_buffer_;
    const FrontendEventCollectorConfig& cfg_;
};

#endif // FRONTEND_COLLECTOR_MODE_H
