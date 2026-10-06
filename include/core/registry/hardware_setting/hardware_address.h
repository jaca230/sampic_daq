#ifndef SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_ADDRESS_H
#define SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_ADDRESS_H

#include <string>

struct HardwareAddress {
    int feb = -1;
    int chip = -1;
    int channel = -1;

    std::string describe() const {
        if (channel >= 0) {
            return "feb" + std::to_string(feb) + "/sampic" +
                   std::to_string(chip) + "/channel" +
                   std::to_string(channel);
        }
        if (chip >= 0) {
            return "feb" + std::to_string(feb) + "/sampic" +
                   std::to_string(chip);
        }
        if (feb >= 0) return "feb" + std::to_string(feb);
        return "crate";
    }
};

#endif
