#ifndef ODB_MANAGER_H
#define ODB_MANAGER_H

#include "midas.h"
#include "odbxx.h"
#include <nlohmann/json.hpp>
#include <rfl/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

using json = nlohmann::json;

class OdbManager {
public:
    OdbManager();

    // ---- JSON/String API ----
    std::string read(const std::string& path);
    json read(const std::string& path, bool return_json_object);

    void write(const std::string& path, const std::string& jsonStr);
    void write(const std::string& path, const json& j);

    void initialize(const std::string& path, const std::string& jsonStr);
    void initialize(const std::string& path, const json& j);

    // ---- Generic Struct API (Reflect-C++) ----
    template <typename T>
    T read(const std::string& path) {
        std::string s = read(path);
        auto parsed = rfl::json::read<T>(s);
        if (!parsed)
            throw std::runtime_error("Failed to deserialize ODB JSON at path: " + path);
        return *parsed;
    }

    template <typename T>
    void write(const std::string& path, const T& obj) {
        try {
            auto j = json::parse(rfl::json::write(obj));
            write(path, j);
        } catch (const std::exception& e) {
            throw std::runtime_error("OdbManager::write failed for path '" + path + "': " + e.what());
        }
    }

    template <typename T>
    void initialize(const std::string& path, const T& obj) {
        try {
            auto j = json::parse(rfl::json::write(obj));
            initialize(path, j);
        } catch (const std::exception& e) {
            throw std::runtime_error("OdbManager::initialize failed for path '" + path + "': " + e.what());
        }
    }

private:
    enum class OdbMode { WRITE, INITIALIZE };
    void populateOdbHelper(midas::odb& odb, const json& j, OdbMode mode);

    void populateOdbFromJson(midas::odb& odb, const json& j);
    void initializeOdbFromJson(midas::odb& odb, const json& j);
    json removeKeysContainingKey(const json& j);
};

#endif // ODB_MANAGER_H
