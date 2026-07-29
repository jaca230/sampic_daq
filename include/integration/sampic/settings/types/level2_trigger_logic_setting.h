#ifndef SAMPIC_DAQ_LEVEL2_TRIGGER_LOGIC_SETTING_H
#define SAMPIC_DAQ_LEVEL2_TRIGGER_LOGIC_SETTING_H

struct Level2TriggerLogicSetting {
    int sel_input0 = 0;
    int sel_input1 = 1;
    int sel_input2 = 2;
    int sel_input3 = 3;
    int layer1_logic0 = 1;
    int layer1_logic1 = 1;
    int layer1_logic2 = 1;
    int layer2_logic0 = 1;
    int layer2_logic1 = 1;
    int layer3_logic = 1;
    bool apply = false;
};

#endif
