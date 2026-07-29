#ifndef SAMPIC_DAQ_CALIBRATION_LOADER_H
#define SAMPIC_DAQ_CALIBRATION_LOADER_H

#include <filesystem>
#include <string_view>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

class CalibrationLoader {
public:
    static std::filesystem::path resolveDirectory(std::string_view configured_path);
    static SAMPIC256CH_ErrCode load(
        CrateInfoStruct& info,
        CrateParamStruct& params,
        std::string_view configured_path);
};

#endif
