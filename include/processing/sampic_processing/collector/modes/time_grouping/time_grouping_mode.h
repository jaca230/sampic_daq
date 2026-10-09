#ifndef FRONTEND_COLLECTOR_MODE_TIME_GROUPING_H
#define FRONTEND_COLLECTOR_MODE_TIME_GROUPING_H

#include "processing/sampic_processing/collector/modes/default/default_mode.h"
#include "processing/sampic_processing/collector/modes/time_grouping/time_grouping_config.h"

class FrontendCollectorModeTimeGrouping : public FrontendCollectorModeDefault {
public:
    FrontendCollectorModeTimeGrouping(
        FrontendCollectorModeContext& context,
        FrontendCollectorModeTimeGroupingConfig config);
};

#endif
