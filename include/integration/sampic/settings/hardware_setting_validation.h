#ifndef SAMPIC_DAQ_HARDWARE_SETTING_VALIDATION_H
#define SAMPIC_DAQ_HARDWARE_SETTING_VALIDATION_H

class HardwareSettingValidation {
public:
    static void positive(int value);
    static void nonnegative(int value);
    static void byteRange(int value);
    static void range(int value, int minimum, int maximum);
    static constexpr auto any = [](const auto&) {};
};

#endif
