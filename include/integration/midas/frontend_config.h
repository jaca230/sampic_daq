#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H

#include <string>
#include <cstddef>

// Configuration for MIDAS frontend integration.
// These parameters can be populated from the ODB.
struct FrontendConfig {
    size_t min_bytes_to_trigger_on = 0;   // minimum event size in bytes
    std::string init_color = "#8A2BE2";      // initial color code for frontend GUI
    std::string ready_color = "greenLight";  // ready status color
    int polling_interval_us = 1000;          // polling interval (microseconds)
    std::string data_bank_prefix = "AD";     // prefix for data banks
    std::string timing_bank_prefix = "AT";   // prefix for timing banks
};

#endif // SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_CONFIG_H
