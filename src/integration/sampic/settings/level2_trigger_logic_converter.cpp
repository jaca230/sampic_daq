#include "integration/sampic/settings/level2_trigger_logic_converter.h"

Level2TriggerLogicSetting Level2TriggerLogicConverter::fromVendor(
    const TriggerLogicParamStruct& value) {
    return {
        value.SelInput0, value.SelInput1, value.SelInput2, value.SelInput3,
        static_cast<int>(value.Layer1TriggerLogic0),
        static_cast<int>(value.Layer1TriggerLogic1),
        static_cast<int>(value.Layer1TriggerLogic2),
        static_cast<int>(value.Layer2TriggerLogic0),
        static_cast<int>(value.Layer2TriggerLogic1),
        static_cast<int>(value.Layer3TriggerLogic), false};
}

TriggerLogicParamStruct Level2TriggerLogicConverter::toVendor(
    const Level2TriggerLogicSetting& value) {
    TriggerLogicParamStruct result{};
    result.SelInput0 = value.sel_input0;
    result.SelInput1 = value.sel_input1;
    result.SelInput2 = value.sel_input2;
    result.SelInput3 = value.sel_input3;
    result.Layer1TriggerLogic0 =
        static_cast<CombiTriggerLogic_t>(value.layer1_logic0);
    result.Layer1TriggerLogic1 =
        static_cast<CombiTriggerLogic_t>(value.layer1_logic1);
    result.Layer1TriggerLogic2 =
        static_cast<CombiTriggerLogic_t>(value.layer1_logic2);
    result.Layer2TriggerLogic0 =
        static_cast<CombiTriggerLogic_t>(value.layer2_logic0);
    result.Layer2TriggerLogic1 =
        static_cast<CombiTriggerLogic_t>(value.layer2_logic1);
    result.Layer3TriggerLogic =
        static_cast<CombiTriggerLogic_t>(value.layer3_logic);
    return result;
}
