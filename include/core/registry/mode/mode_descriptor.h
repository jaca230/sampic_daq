#ifndef SAMPIC_DAQ_CORE_REGISTRY_MODE_DESCRIPTOR_H
#define SAMPIC_DAQ_CORE_REGISTRY_MODE_DESCRIPTOR_H

#include "core/config/config_store.h"

#include <functional>
#include <memory>
#include <string>

template <typename Base, typename Context>
struct ModeDescriptor {
    std::string id;
    std::string description;
    std::function<void(ConfigStore&, const std::string&)> initialize;
    std::function<std::unique_ptr<Base>(
        Context&, const ConfigStore&, const std::string&)> create;
};

#endif
