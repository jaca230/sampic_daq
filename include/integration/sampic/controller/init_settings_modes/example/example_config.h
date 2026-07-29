#ifndef SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_CONFIG_H
#define SAMPIC_INIT_SETTINGS_MODE_EXAMPLE_CONFIG_H

#include <string>

struct SampicInitSettingsModeExampleConfig {
    std::string ip_address = "192.168.0.4";
    int port = 27015;
    int connection_type = 1;
    int control_type = 1;
    std::string calibration_directory = "resources/calib";
};

#endif
