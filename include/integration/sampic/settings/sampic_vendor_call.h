#ifndef SAMPIC_DAQ_SAMPIC_VENDOR_CALL_H
#define SAMPIC_DAQ_SAMPIC_VENDOR_CALL_H

#include "core/registry/hardware_setting/hardware_address.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

class SampicVendorCall {
public:
    static void check(SAMPIC256CH_ErrCode code,
                      const char* operation,
                      const HardwareAddress& address);
};

#endif
