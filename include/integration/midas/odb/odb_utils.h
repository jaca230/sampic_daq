#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_ODB_UTILS_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_ODB_UTILS_H

#include "midas.h"
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

extern HNDLE hDB;

class OdbUtils {
public:
    // -------------------------------
    // Common frontend helpers
    // -------------------------------
    static void odbSetStatusColor(int frontend_index, const std::string& color) {
        char path[256];
        snprintf(path, sizeof(path),
                 "/Equipment/SAMPIC %02d/Common/Status color", frontend_index);

        if (db_set_value(hDB, 0, path, color.c_str(),
                         32, 1, TID_STRING) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set status color '{}' at '{}'", color, path);
        }
    }

    static void odbSetStatusMessage(int frontend_index, const std::string& message) {
        char path[256];
        snprintf(path, sizeof(path),
                 "/Equipment/SAMPIC %02d/Common/Status", frontend_index);

        if (db_set_value(hDB, 0, path, message.c_str(),
                         256, 1, TID_STRING) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set status message '{}' at '{}'", message, path);
        }
    }

    // -------------------------------
    // Generic scalar setters
    // -------------------------------
    static void odbSetString(const std::string& path, const std::string& value) {
        if (db_set_value(hDB, 0, path.c_str(),
                         value.c_str(), value.size() + 1, 1, TID_STRING) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set string '{}={}'", path, value);
        }
    }

    static void odbSetInt(const std::string& path, int value) {
        if (db_set_value(hDB, 0, path.c_str(),
                         &value, sizeof(value), 1, TID_INT32) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set int '{}={}'", path, value);
        }
    }

    static void odbSetBool(const std::string& path, bool value) {
        int v = value ? 1 : 0;
        if (db_set_value(hDB, 0, path.c_str(),
                         &v, sizeof(v), 1, TID_BOOL) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set bool '{}={}'", path, value);
        }
    }

    static void odbSetDouble(const std::string& path, double value) {
        if (db_set_value(hDB, 0, path.c_str(),
                         &value, sizeof(value), 1, TID_DOUBLE) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set double '{}={}'", path, value);
        }
    }

    // -------------------------------
    // Array setters
    // -------------------------------
    static void odbSetIntArray(const std::string& path, const std::vector<int>& values) {
        if (db_set_value(hDB, 0, path.c_str(),
                         values.data(), values.size() * sizeof(int),
                         values.size(), TID_INT32) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set int array at '{}'", path);
        }
    }

    static void odbSetDoubleArray(const std::string& path, const std::vector<double>& values) {
        if (db_set_value(hDB, 0, path.c_str(),
                         values.data(), values.size() * sizeof(double),
                         values.size(), TID_DOUBLE) != DB_SUCCESS) {
            spdlog::error("ODB: Failed to set double array at '{}'", path);
        }
    }

    static void odbSetStringArray(const std::string& path, const std::vector<std::string>& values) {
        for (size_t i = 0; i < values.size(); ++i) {
            std::string itemPath = path + "/" + std::to_string(i);
            if (db_set_value(hDB, 0, itemPath.c_str(),
                             values[i].c_str(), values[i].size() + 1,
                             1, TID_STRING) != DB_SUCCESS) {
                spdlog::error("ODB: Failed to set string array element '{}={}'", itemPath, values[i]);
            }
        }
    }
};

#endif // SAMPIC_DAQ_INTEGRATION_MIDAS_ODB_UTILS_H
