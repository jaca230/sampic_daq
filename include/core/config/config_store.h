#ifndef SAMPIC_DAQ_CORE_CONFIG_CONFIG_STORE_H
#define SAMPIC_DAQ_CORE_CONFIG_CONFIG_STORE_H

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

class ConfigStore {
public:
    using Json = nlohmann::json;

    virtual ~ConfigStore() = default;
    virtual Json readJson(const std::string& path) const = 0;
    virtual void writeJson(const std::string& path, const Json& value) = 0;
    virtual void initializeJson(const std::string& path, const Json& default_value) = 0;

    template <typename T>
    T readValue(const std::string& path) const {
        const auto value = readJson(path);
        if (value.is_null()) {
            throw std::runtime_error("configuration value is missing at '" + path + "'");
        }
        try {
            return value.template get<T>();
        } catch (const std::exception& error) {
            throw std::runtime_error("invalid configuration value at '" + path +
                                     "': " + error.what());
        }
    }

    template <typename T>
    void initializeValue(const std::string& path, const T& default_value) {
        initializeJson(path, Json(default_value));
    }
};

#endif
