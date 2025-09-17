#include "odb/odb_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>

using json = nlohmann::json;

// Constructor
OdbManager::OdbManager() {
    // Any initialization code if needed
}

// Public method to read and return JSON string from ODB
std::string OdbManager::read(const std::string& path) {
    // Call the overloaded method with return_json_object = false to get JSON string
    json result = read(path, false);
    return result.dump();
}

// Overloaded method to read and return JSON object from ODB
// The boolean is an overload trick
json OdbManager::read(const std::string& path, bool return_json_object) {
    midas::odb o(path);  // Connect to ODB path

    // Retrieve the ODB structure as a JSON string
    std::string jsonString = o.dump();

    // Parse the JSON string into a JSON object
    json parsedJson = json::parse(jsonString);

    // Create a new JSON object to hold the cleaned data (removes "/key")
    json cleanedJson = removeKeysContainingKey(parsedJson);

    json secondLevelJson;

    // Iterate through the cleaned JSON and collect second-level objects
    // Skip the very top-level key (since it's not relevant)
    for (auto& [key, value] : cleanedJson.items()) {
        // Now clean the second-level object (skip /key entries)
        for (auto& [subkey, subvalue] : value.items()) {
            secondLevelJson[subkey] = subvalue;
        }
    }

    return secondLevelJson;
}


// Public method to write JSON string to ODB
void OdbManager::write(const std::string& path, const std::string& jsonStr) {
    // Convert the string to JSON object and call the overload that takes JSON object
    json j = json::parse(jsonStr);
    write(path, j);
}

// Overloaded method to write JSON object to ODB
void OdbManager::write(const std::string& path, const json& j) {
    // Connect to the ODB path
    midas::odb o;
    o.connect(path);

    // Use the helper method to recursively populate the ODB from the JSON object
    populateOdbFromJson(o, j);

    // Push the settings to the ODB
    o.write();
}

// Public method to initialize ODB only if keys do not exist
void OdbManager::initialize(const std::string& path, const std::string& jsonStr) {
    // Convert the string to JSON object and call the overload that takes JSON object
    json j = json::parse(jsonStr);
    initialize(path, j);
}

// Overloaded method to initialize ODB using JSON object
void OdbManager::initialize(const std::string& path, const json& j) {
    // Connect to the ODB path
    midas::odb o;
    o.connect(path);

    // Use the helper method to populate only missing values
    initializeOdbFromJson(o, j);
}

// Helper function to populate ODB only if keys don't exist
void OdbManager::initializeOdbFromJson(midas::odb& odb, const json& j) {
    for (auto& [key, value] : j.items()) {
        std::string fullKeyPath = odb.get_full_path() + "/" + key;  // Get full path for key
        if (!odb.exists(fullKeyPath)) {  
            if (value.is_object()) {
                // Recursively initialize sub-objects
                midas::odb subodb;
                subodb.connect(fullKeyPath);
                initializeOdbFromJson(subodb, value);
            } else if (value.is_array()) {
                // Handle arrays
                if (value.size() > 0 && value[0].is_string()) {
                    odb[key] = value.get<std::vector<std::string>>(); 
                } else if (value.size() > 0 && value[0].is_number_integer()) {
                    odb[key] = value.get<std::vector<int>>(); 
                } else if (value.size() > 0 && value[0].is_boolean()) {
                    odb[key] = value.get<std::vector<bool>>(); 
                } else {
                    std::cerr << "[ERROR] Unsupported array type in JSON for key: " << fullKeyPath << std::endl;
                }
            } else {
                // Handle primitive types
                if (value.is_number_integer()) {
                    odb[key] = value.get<int>(); 
                } else if (value.is_boolean()) {
                    odb[key] = value.get<bool>(); 
                } else if (value.is_string()) {
                    odb[key] = value.get<std::string>(); 
                } else {
                    std::cerr << "[ERROR] Unsupported data type for key: " << fullKeyPath << std::endl;
                }
            }
        }
    }
}

// Helper function to recursively populate ODB from JSON object
void OdbManager::populateOdbFromJson(midas::odb& odb, const json& j) {
    // Iterate through the JSON object and populate the ODB structure
    for (auto& [key, value] : j.items()) {
        if (value.is_object()) {
            // If the value is an object, recursively add it to the ODB
            midas::odb subodb;
            subodb.connect(odb.get_full_path() + "/" + key); // Connect to the sub-path
            populateOdbFromJson(subodb, value);  // Recursive call for nested objects
        } else if (value.is_array()) {
            // Handle arrays: determine type and populate accordingly
            if (value.size() > 0 && value[0].is_string()) {
                // Array of strings
                odb[key] = value.get<std::vector<std::string>>(); 
            } else if (value.size() > 0 && value[0].is_number_integer()) {
                // Array of integers
                odb[key] = value.get<std::vector<int>>(); 
            } else if (value.size() > 0 && value[0].is_boolean()) {
                // Array of booleans
                odb[key] = value.get<std::vector<bool>>(); 
            } else {
                // Handle more complex cases if necessary
                std::cerr << "Unsupported array type in JSON." << std::endl;
            }
        } else {
            // For other data types like int, bool, string, etc.
            if (value.is_number_integer()) {
                odb[key] = value.get<int>(); 
            } else if (value.is_boolean()) {
                odb[key] = value.get<bool>(); 
            } else if (value.is_string()) {
                odb[key] = value.get<std::string>(); 
            } else {
                // Handle other types if necessary
                std::cerr << "Unsupported data type for key: " << key << std::endl;
            }
        }
    }
}

// Helper function to recursively remove keys containing "/key"
json OdbManager::removeKeysContainingKey(const json& j) {
    json result = j;  // Start with the input JSON object

    // Traverse each element in the JSON object
    for (auto it = result.begin(); it != result.end(); ) {
        // If the key contains "/key", erase it from the result
        if (it.key().find("/key") != std::string::npos) {
            it = result.erase(it);
        } else {
            // If the value is a nested object or array, recurse
            if (it.value().is_object() || it.value().is_array()) {
                it.value() = removeKeysContainingKey(it.value());
            }
            ++it;
        }
    }

    return result;
}