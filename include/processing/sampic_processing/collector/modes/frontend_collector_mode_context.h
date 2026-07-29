#ifndef SAMPIC_DAQ_FRONTEND_COLLECTOR_MODE_CONTEXT_H
#define SAMPIC_DAQ_FRONTEND_COLLECTOR_MODE_CONTEXT_H

#include "integration/sampic/collector/sampic_event_buffer.h"
#include "processing/sampic_processing/collector/frontend_diagnostics.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/config/frontend_collector_diagnostics_config.h"

struct FrontendCollectorModeContext {
    SampicEventBuffer& input;
    FrontendEventBuffer& output;
    FrontendCollectorDiagnosticsConfig diagnostics_config;
    frontend::collector::FrontendDiagnostics& diagnostics;
};

#endif
