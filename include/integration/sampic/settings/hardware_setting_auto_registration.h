#ifndef SAMPIC_DAQ_HARDWARE_SETTING_AUTO_REGISTRATION_H
#define SAMPIC_DAQ_HARDWARE_SETTING_AUTO_REGISTRATION_H

#include "integration/sampic/settings/sampic_hardware_registry.h"

#ifndef SAMPIC_CONCAT
#define SAMPIC_CONCAT_IMPL(left, right) left##right
#define SAMPIC_CONCAT(left, right) SAMPIC_CONCAT_IMPL(left, right)
#endif

#define SAMPIC_REGISTER_HARDWARE_SETTINGS(Provider)                         \
    static const SampicHardwareRegistry::Registration<Provider>            \
        SAMPIC_CONCAT(sampic_hardware_registration_, __COUNTER__){}

#endif
