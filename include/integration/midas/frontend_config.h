#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H

#include <string>
#include <cstddef>

// Configuration for MIDAS frontend integration.
// These parameters can be populated from the ODB.
struct FrontendConfig {
    std::string init_color = "#8A2BE2";      // initial color code for frontend GUI
    std::string ready_color = "greenLight";  // ready status color
    int polling_interval_us = 1000000;       // microseconds between polling for new data
};

#endif // SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H
