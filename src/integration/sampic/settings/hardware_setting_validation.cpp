#include "integration/sampic/settings/hardware_setting_validation.h"

#include <stdexcept>
#include <string>

void HardwareSettingValidation::positive(int value) {
    if (value <= 0) throw std::invalid_argument("must be positive");
}

void HardwareSettingValidation::nonnegative(int value) {
    if (value < 0) throw std::invalid_argument("must be non-negative");
}

void HardwareSettingValidation::byteRange(int value) {
    range(value, 0, 255);
}

void HardwareSettingValidation::range(
    int value, int minimum, int maximum) {
    if (value < minimum || value > maximum) {
        throw std::invalid_argument(
            "must be in [" + std::to_string(minimum) + "," +
            std::to_string(maximum) + "]");
    }
}
