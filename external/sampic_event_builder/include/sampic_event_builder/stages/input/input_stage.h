#pragma once

#include "sampic_event_builder/model/hit.h"
#include "sampic_event_builder/pipeline/stage.h"
#include <vector>

namespace sampic::event_builder {

class InputStage : public Stage {
public:
    virtual std::vector<Hit> read_available() = 0;
};

} // namespace sampic::event_builder
