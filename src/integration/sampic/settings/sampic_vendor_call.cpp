#include "integration/sampic/settings/sampic_vendor_call.h"

#include <stdexcept>
#include <string>

void SampicVendorCall::check(SAMPIC256CH_ErrCode code,
                             const char* operation,
                             const HardwareAddress& address) {
    if (code != SAMPIC256CH_Success) {
        throw std::runtime_error(
            std::string(operation) + " failed at " + address.describe() +
            " (vendor code " + std::to_string(static_cast<int>(code)) + ")");
    }
}
