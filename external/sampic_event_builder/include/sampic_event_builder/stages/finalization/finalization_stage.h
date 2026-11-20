#pragma once

#include "sampic_event_builder/pipeline/pipeline_event.h"
#include "sampic_event_builder/pipeline/stage.h"

namespace sampic::event_builder {

class FinalizationStage : public Stage {
public:
    virtual FinalizedEvent finalize(const PipelineEvent& event) const = 0;
};

} // namespace sampic::event_builder
