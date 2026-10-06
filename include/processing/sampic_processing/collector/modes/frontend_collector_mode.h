#ifndef FRONTEND_COLLECTOR_MODE_H
#define FRONTEND_COLLECTOR_MODE_H

#include <memory>
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/collector/frontend_diagnostics.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "integration/sampic/collector/sampic_event_buffer.h"
#include "core/registry/mode/mode_registry.h"
#include "processing/sampic_processing/collector/modes/frontend_collector_mode_context.h"

/**
 * @brief Abstract base class for frontend collector modes.
 * Each mode defines how SAMPIC events are transformed into FrontendEvents.
 */
class FrontendCollectorMode {
public:
    explicit FrontendCollectorMode(FrontendCollectorModeContext& context)
        : sampic_buffer_(context.input),
          frontend_buffer_(context.output),
          diagnostics_config_(std::move(context.diagnostics_config)),
          diagnostics_(context.diagnostics) {}

    virtual ~FrontendCollectorMode() = default;

    /**
     * @brief Perform one data collection cycle.
     * The mode may fetch new SampicEvents from sampic_buffer_ and push
     * one or more FrontendEvents into frontend_buffer_.
     * @return true if succeeded (no fatal error).
     */
    virtual bool collect() = 0;

    /**
     * @brief Finalize mode-local state after the input buffer is drained.
     *
     * Modes without pending aggregation state need no special action.
     */
    virtual bool flush() { return true; }

protected:
    SampicEventBuffer& sampic_buffer_;
    FrontendEventBuffer& frontend_buffer_;
    FrontendCollectorDiagnosticsConfig diagnostics_config_;
    frontend::collector::FrontendDiagnostics& diagnostics_;
};

using FrontendCollectorModeRegistry =
    ModeRegistry<FrontendCollectorMode, FrontendCollectorModeContext>;

#endif // FRONTEND_COLLECTOR_MODE_H
