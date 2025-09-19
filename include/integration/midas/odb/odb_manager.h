#ifndef ODB_MANAGER_H
#define ODB_MANAGER_H

#include "midas.h"
#include <nlohmann/json.hpp>
#include <rfl/json.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

using json = nlohmann::json;

extern HNDLE hDB;

class OdbManager {
public:
    explicit OdbManager(HNDLE handle = hDB) : hDB_handle(handle) {}

    // JSON/String API
    std::string read(const std::string& path);
    json read(const std::string& path, bool return_json_object);

    void write(const std::string& path, const std::string& jsonStr);
    void write(const std::string& path, const json& j);

    void initialize(const std::string& path, const std::string& jsonStr);
    void initialize(const std::string& path, const json& j);

    // Generic template API (Reflect-C++)
    template <typename T>
    T read(const std::string& path) {
        std::string jsonStr = read(path);

        auto parsed = rfl::json::read<T>(jsonStr);
        if (!parsed.has_value()) {
            spdlog::error("Failed to deserialize ODB JSON at path '{}'", path);
            throw std::runtime_error("Failed to deserialize ODB JSON at path: " + path);
        }
        return parsed.value();
    }

    template <typename T>
    void write(const std::string& path, const T& obj) {
        auto j = json::parse(rfl::json::write(obj));
        write(path, j);
    }

    template <typename T>
    void initialize(const std::string& path, const T& obj) {
        auto j = json::parse(rfl::json::write(obj));
        initialize(path, j);
    }

private:
    HNDLE hDB_handle;

    enum class OdbMode { WRITE, INITIALIZE };
    void populateOdbHelper(const std::string& basePath, const json& j, OdbMode mode);

    json removeKeysContainingKey(const json& j);
    json readRecursive(HNDLE key, const std::string& fullPath);
};

#endif // ODB_MANAGER_H
