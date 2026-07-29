#ifndef SAMPIC_DAQ_CORE_CONFIG_MEMORY_CONFIG_STORE_H
#define SAMPIC_DAQ_CORE_CONFIG_MEMORY_CONFIG_STORE_H

#include "core/config/config_store.h"

#include <unordered_map>

class MemoryConfigStore final : public ConfigStore {
public:
    Json readJson(const std::string& path) const override {
        const auto it = values_.find(path);
        return it == values_.end() ? Json(nullptr) : it->second;
    }

    void writeJson(const std::string& path, const Json& value) override {
        values_[path] = value;
    }

    void initializeJson(
        const std::string& path, const Json& default_value) override {
        values_.try_emplace(path, default_value);
    }

private:
    std::unordered_map<std::string, Json> values_;
};

#endif
