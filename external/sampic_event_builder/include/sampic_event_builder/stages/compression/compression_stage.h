#pragma once

#include "sampic_event_builder/pipeline/pipeline_event.h"
#include "sampic_event_builder/pipeline/stage.h"

namespace sampic::event_builder {

class CompressionStage : public Stage {
public:
    virtual void compress(PipelineEvent& event) const = 0;
};

} // namespace sampic::event_builder
