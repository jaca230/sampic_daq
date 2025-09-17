#include "odb/odb_manager.h"
#include <cstring>
#include <stdexcept>

// --- JSON/String API ---
std::string OdbManager::read(const std::string& path) {
    json j = read(path, false);
    return j.dump();
}

json OdbManager::read(const std::string& path, bool /*unused*/) {
    HNDLE key;
    if (db_find_key(hDB_handle, 0, path.c_str(), &key) != DB_SUCCESS) {
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
        return result; // already recursed through all children

    // Scalar leaf keys
    switch (type) {
        case TID_UINT8: case TID_INT8: case TID_UINT16: case TID_INT16:
        case TID_UINT32: case TID_INT32: case TID_INT64: case TID_UINT64: {
            int64_t val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = val;
            break;
        }
        case TID_FLOAT32: case TID_FLOAT64: {
            double val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = val;
            break;
        }
        case TID_BOOL: {
            int val = 0;
            int read_size = item_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), &val, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = (val != 0);
            break;
        }
        case TID_STRING: {
            int buf_size = std::max(item_size, 1);
            char* buf = new char[buf_size]();
            int read_size = buf_size;
            ret = db_get_value(hDB_handle, 0, fullPath.c_str(), buf, &read_size, type, FALSE);
            if (ret == DB_SUCCESS) result = std::string(buf, read_size);
            delete[] buf;
            break;
        }
        default:
            break;
    }

    return result;
}

// --- Write / Initialize API ---
void OdbManager::write(const std::string& path, const std::string& jsonStr) {
    write(path, json::parse(jsonStr));
}

void OdbManager::write(const std::string& path, const json& j) {
    populateOdbHelper(path, j, OdbMode::WRITE);
}

void OdbManager::initialize(const std::string& path, const std::string& jsonStr) {
    initialize(path, json::parse(jsonStr));
}

void OdbManager::initialize(const std::string& path, const json& j) {
    populateOdbHelper(path, j, OdbMode::INITIALIZE);
}

// --- Populate ODB recursively ---
void OdbManager::populateOdbHelper(const std::string& basePath, const json& j, OdbMode mode) {
    for (auto& [key, value] : j.items()) {
        std::string fullPath = basePath + "/" + key;

        if (mode == OdbMode::INITIALIZE) {
            HNDLE subkey;
            if (db_find_key(hDB_handle, 0, fullPath.c_str(), &subkey) == DB_SUCCESS)
                continue;
        }

        if (value.is_object()) {
            populateOdbHelper(fullPath, value, mode);
        } else if (value.is_array()) {
            if (value.empty()) continue;
            for (size_t i = 0; i < value.size(); ++i) {
                std::string itemPath = fullPath + "/" + std::to_string(i);
                if (value[i].is_string()) {
                    std::string s = value[i];
                    db_set_value(hDB_handle, 0, itemPath.c_str(), s.c_str(), s.size()+1, 1, TID_STRING);
                } else if (value[i].is_boolean()) {
                    int b = value[i].get<bool>() ? 1 : 0;
                    db_set_value(hDB_handle, 0, itemPath.c_str(), &b, sizeof(int), 1, TID_BOOL);
                } else if (value[i].is_number_float()) {
                    double d = value[i];
                    db_set_value(hDB_handle, 0, itemPath.c_str(), &d, sizeof(double), 1, TID_FLOAT64);
                } else if (value[i].is_number_integer()) {
                    int64_t v = value[i];
                    db_set_value(hDB_handle, 0, itemPath.c_str(), &v, sizeof(int64_t), 1, TID_INT64);
                }
            }
        } else {
            if (value.is_string()) {
                std::string s = value;
                db_set_value(hDB_handle, 0, fullPath.c_str(), s.c_str(), s.size()+1, 1, TID_STRING);
            } else if (value.is_boolean()) {
                int b = value.get<bool>() ? 1 : 0;
                db_set_value(hDB_handle, 0, fullPath.c_str(), &b, sizeof(int), 1, TID_BOOL);
            } else if (value.is_number_float()) {
                double d = value.get<double>();
                db_set_value(hDB_handle, 0, fullPath.c_str(), &d, sizeof(double), 1, TID_FLOAT64);
            } else if (value.is_number_integer()) {
                int64_t v = value.get<int64_t>();
                db_set_value(hDB_handle, 0, fullPath.c_str(), &v, sizeof(int64_t), 1, TID_INT64);
            }
        }
    }
}

// --- Utility ---
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
