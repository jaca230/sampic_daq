#include "integration/midas/odb/odb_manager.h"
#include <cstring>

// --- JSON/String API ---
std::string OdbManager::read(const std::string& path) {
    json j = read(path, false);
    return j.dump();
}

json OdbManager::read(const std::string& path, bool /*unused*/) {
    HNDLE key;
    if (db_find_key(hDB_handle, 0, path.c_str(), &key) != DB_SUCCESS) {
        // Commented out because this is sometimes expected behavior
        // spdlog::warn("ODB key '{}' not found", path);
        return nullptr;
    }
    return readRecursive(key, path);
}

// --- Recursive reader with full path ---
json OdbManager::readRecursive(HNDLE key, const std::string& fullPath) {
    json result;

    INT type = 0, num_values = 0, item_size = 0;
    char name[128] = {0};

    INT ret = db_get_key_info(hDB_handle, key, name, sizeof(name), &type, &num_values, &item_size);
    if (ret != DB_SUCCESS) {
        spdlog::error("db_get_key_info failed for path '{}'", fullPath);
        return result;
    }

    // Container keys (struct/array) or any node with subkeys
    HNDLE subkey;
    INT idx = 0;
    bool hasSubkeys = false;
    while (db_enum_key(hDB_handle, key, idx, &subkey) == DB_SUCCESS) {
        hasSubkeys = true;
        char subname[128] = {0};
        INT sub_type = 0, sub_num_values = 0, sub_item_size = 0;
        if (db_get_key_info(hDB_handle, subkey, subname, sizeof(subname),
                            &sub_type, &sub_num_values, &sub_item_size) == DB_SUCCESS) {
            std::string subPath = fullPath + "/" + subname;
            result[subname] = readRecursive(subkey, subPath);
        }
        ++idx;
    }

    if (hasSubkeys)
        return result;

    // Scalar leaf keys
    switch (type) {
        case TID_UINT8: case TID_INT8: case TID_UINT16: case TID_INT16:
        case TID_UINT32: case TID_INT32: case TID_INT64: case TID_UINT64: {
            int64_t val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = val;
            else spdlog::error("db_get_value failed for path '{}' (int)", fullPath);
            break;
        }
        case TID_FLOAT32: case TID_FLOAT64: {
            double val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = val;
            else spdlog::error("db_get_value failed for path '{}' (float)", fullPath);
            break;
        }
        case TID_BOOL: {
            int val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = (val != 0);
            else spdlog::error("db_get_value failed for path '{}' (bool)", fullPath);
            break;
        }
        case TID_STRING: {
            int buf_size = std::max(item_size, 1);
            std::vector<char> buf(buf_size, 0);
            int read_size = buf_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), buf.data(), &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = std::string(buf.data(), read_size);
            else spdlog::error("db_get_value failed for path '{}' (string)", fullPath);
            break;
        }
        default:
            spdlog::warn("Unsupported ODB type {} at path '{}'", type, fullPath);
            break;
    }

    return result;
}

// --- Write / Initialize API ---
void OdbManager::write(const std::string& path, const std::string& jsonStr) {
    write(path, json::parse(jsonStr));
}

void OdbManager::write(const std::string& path, const json& j) {
    spdlog::info("Writing to ODB at '{}'", path);
    populateOdbHelper(path, j, OdbMode::WRITE);
}

void OdbManager::initialize(const std::string& path, const std::string& jsonStr) {
    initialize(path, json::parse(jsonStr));
}

void OdbManager::initialize(const std::string& path, const json& j) {
    // Don't need this info, plus we need to call this before logger is initialized
    // spdlog::info("Initializing ODB at '{}'", path); 
    populateOdbHelper(path, j, OdbMode::INITIALIZE);
}

// --- Populate ODB recursively ---
void OdbManager::populateOdbHelper(const std::string& basePath, const json& j, OdbMode mode) {
    if (j.is_object()) {
        // Recurse on object fields
        for (auto& [key, value] : j.items()) {
            std::string fullPath = basePath + "/" + key;
            if (mode == OdbMode::INITIALIZE) {
                HNDLE subkey;
                if (db_find_key(hDB_handle, 0, fullPath.c_str(), &subkey) == DB_SUCCESS)
                    continue; // already exists
            }
            populateOdbHelper(fullPath, value, mode);
        }
    }
    else if (j.is_array()) {
        if (j.empty()) return;

        // Detect type of first element
        if (j.front().is_primitive()) {
            // Treat as real ODB array of primitives
            if (j.front().is_string()) {
                std::vector<std::string> vals;
                for (auto& v : j) vals.push_back(v.get<std::string>());
                for (size_t i = 0; i < vals.size(); i++) {
                    std::string itemPath = basePath + "/" + std::to_string(i);
                    db_set_value(hDB_handle, 0, itemPath.c_str(),
                                 vals[i].c_str(), vals[i].size()+1, 1, TID_STRING);
                }
            }
            else if (j.front().is_boolean()) {
                std::vector<int> vals;
                for (auto& v : j) vals.push_back(v.get<bool>() ? 1 : 0);
                db_set_value(hDB_handle, 0, basePath.c_str(),
                             vals.data(), sizeof(int)*vals.size(), vals.size(), TID_BOOL);
            }
            else if (j.front().is_number_float()) {
                std::vector<double> vals;
                for (auto& v : j) vals.push_back(v.get<double>());
                db_set_value(hDB_handle, 0, basePath.c_str(),
                             vals.data(), sizeof(double)*vals.size(), vals.size(), TID_FLOAT64);
            }
            else if (j.front().is_number_integer()) {
                std::vector<int64_t> vals;
                for (auto& v : j) vals.push_back(v.get<int64_t>());
                db_set_value(hDB_handle, 0, basePath.c_str(),
                             vals.data(), sizeof(int64_t)*vals.size(), vals.size(), TID_INT64);
            }
        } else {
            // Array of objects or nested arrays → recurse with indices
            for (size_t i = 0; i < j.size(); ++i) {
                std::string itemPath = basePath + "/" + std::to_string(i);
                populateOdbHelper(itemPath, j[i], mode);
            }
        }
    }
    else if (j.is_primitive()) {
        // Scalars
        if (j.is_string()) {
            std::string s = j;
            db_set_value(hDB_handle, 0, basePath.c_str(),
                         s.c_str(), s.size()+1, 1, TID_STRING);
        }
        else if (j.is_boolean()) {
            int b = j.get<bool>() ? 1 : 0;
            db_set_value(hDB_handle, 0, basePath.c_str(),
                         &b, sizeof(int), 1, TID_BOOL);
        }
        else if (j.is_number_float()) {
            double d = j.get<double>();
            db_set_value(hDB_handle, 0, basePath.c_str(),
                         &d, sizeof(double), 1, TID_FLOAT64);
        }
        else if (j.is_number_integer()) {
            int64_t v = j.get<int64_t>();
            db_set_value(hDB_handle, 0, basePath.c_str(),
                         &v, sizeof(int64_t), 1, TID_INT64);
        }
    }
}


// --- Utility ---
json OdbManager::removeKeysContainingKey(const json& j) {
    json result = j;
    for (auto it = result.begin(); it != result.end(); ) {
        if (it.key().find("/key") != std::string::npos) {
            spdlog::debug("Removing JSON key '{}'", it.key());
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
