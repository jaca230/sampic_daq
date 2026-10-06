#ifndef SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_SETTING_DESCRIPTOR_H
#define SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_SETTING_DESCRIPTOR_H

#include "core/config/config_store.h"
#include "core/registry/hardware_setting/hardware_address.h"
#include "core/registry/hardware_setting/hardware_level.h"

#include <functional>
#include <string>

template <typename VendorContext>
struct HardwareSettingDescriptor {
    std::string id;
    HardwareLevel level;
    std::string odb_key;
    int priority = 0;
    ConfigStore::Json default_value;
    std::function<void(const ConfigStore::Json&)> validate;
    std::function<ConfigStore::Json(const HardwareAddress&, VendorContext&)> getter;
    std::function<void(
        const HardwareAddress&, const ConfigStore::Json&, VendorContext&)> setter;
    std::function<bool(
        const ConfigStore::Json&, const ConfigStore::Json&)> equivalent;
};

#endif
