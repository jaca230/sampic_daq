#include "odb/odb_manager.h"
#include <chrono>

using json = nlohmann::json;

OdbManager::OdbManager() {}

// --- JSON/String API ---

std::string OdbManager::read(const std::string& path) {
    json result = read(path, false);
    return result.dump();
}

json OdbManager::read(const std::string& path, bool /*unused*/) {
    midas::odb o(path);
    std::string jsonString = o.dump();
    json parsedJson = json::parse(jsonString);
    json cleanedJson = removeKeysContainingKey(parsedJson);

    // Flatten second-level objects
    json secondLevelJson;
    for (auto& [key, value] : cleanedJson.items()) {
        for (auto& [subkey, subvalue] : value.items()) {
            secondLevelJson[subkey] = subvalue;
        }
    }
    return secondLevelJson;
}

void OdbManager::write(const std::string& path, const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    write(path, j);
}

void OdbManager::write(const std::string& path, const json& j) {
    midas::odb o;
    o.connect(path); // Timed at ~30s for large ODB sections
    populateOdbFromJson(o, j);
    o.write();
}

void OdbManager::initialize(const std::string& path, const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    initialize(path, j);
}

void OdbManager::initialize(const std::string& path, const json& j) {
    midas::odb o;
    o.connect(path); // Timed at ~30s for large ODB sections
    initializeOdbFromJson(o, j);
}

// --- Consolidated helper ---
void OdbManager::populateOdbHelper(midas::odb& odb, const json& j, OdbMode mode) {
    for (auto& [key, value] : j.items()) {
        std::string fullKeyPath = odb.get_full_path() + "/" + key;

        if (mode == OdbMode::INITIALIZE && odb.exists(fullKeyPath))
            continue;

        if (value.is_object()) {
            midas::odb subodb;
            subodb.connect(fullKeyPath);
            populateOdbHelper(subodb, value, mode);
        }
        else if (value.is_array()) {
            if (value.empty()) continue;

            if (value[0].is_string()) odb[key] = value.get<std::vector<std::string>>();
            else if (value[0].is_number_integer()) odb[key] = value.get<std::vector<int>>();
            else if (value[0].is_number_float()) odb[key] = value.get<std::vector<float>>();
            else if (value[0].is_boolean()) odb[key] = value.get<std::vector<bool>>();
            else std::cerr << "[ERROR] Unsupported array type for key: " << fullKeyPath << "\n";
        }
        else {
            if (value.is_number_integer()) {
                int v = value.get<int>();
                if (v >= 0 && v <= 255) odb[key] = static_cast<uint8_t>(v);
                else if (v >= 0 && v <= 65535) odb[key] = static_cast<uint16_t>(v);
                else if (v >= 0) odb[key] = static_cast<uint32_t>(v);
                else odb[key] = v;
            }
            else if (value.is_number_float()) odb[key] = value.get<float>();
            else if (value.is_boolean()) odb[key] = value.get<bool>();
            else if (value.is_string()) odb[key] = value.get<std::string>();
            else std::cerr << "[ERROR] Unsupported primitive type for key: " << fullKeyPath << "\n";
        }
    }
}

void OdbManager::populateOdbFromJson(midas::odb& odb, const json& j) {
    populateOdbHelper(odb, j, OdbMode::WRITE);
}

void OdbManager::initializeOdbFromJson(midas::odb& odb, const json& j) {
    populateOdbHelper(odb, j, OdbMode::INITIALIZE);
}

// --- Utility to remove "/key" entries ---
json OdbManager::removeKeysContainingKey(const json& j) {
    json result = j;
    for (auto it = result.begin(); it != result.end(); ) {
        if (it.key().find("/key") != std::string::npos) {
            it = result.erase(it);
        } else {
            if (it.value().is_object() || it.value().is_array()) {
                it.value() = removeKeysContainingKey(it.value());
            }
            ++it;
        }
    }
    return result;
}
