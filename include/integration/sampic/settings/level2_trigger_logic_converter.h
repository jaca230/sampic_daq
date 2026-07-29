#ifndef SAMPIC_DAQ_LEVEL2_TRIGGER_LOGIC_CONVERTER_H
#define SAMPIC_DAQ_LEVEL2_TRIGGER_LOGIC_CONVERTER_H

#include "integration/sampic/settings/types/level2_trigger_logic_setting.h"

extern "C" {
#include <SAMPIC_256Ch_Type.h>
}

class Level2TriggerLogicConverter {
public:
    static Level2TriggerLogicSetting fromVendor(
        const TriggerLogicParamStruct& value);
    static TriggerLogicParamStruct toVendor(
        const Level2TriggerLogicSetting& value);
};

#endif
