#include "integration/sampic/sampic_hardware.h"

SampicHardware::SampicHardware()
    : configurator_(info_, params_, eventBuffer_, mlFrames_) {}

void SampicHardware::initialize(const SampicSystemSettings& settings) {
    configurator_.initializeHardware(settings);
}

void SampicHardware::applySettings(const SampicSystemSettings& settings) {
    configurator_.applySettings(settings);
}
