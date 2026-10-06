#include "integration/sampic/calibration/calibration_loader.h"

#include <array>
#include <cstdio>
#include <stdexcept>

#include <spdlog/spdlog.h>

std::filesystem::path CalibrationLoader::resolveDirectory(
    std::string_view configured_path) {
    if (configured_path.empty()) {
        throw std::invalid_argument("calibration directory is empty");
    }

    const std::filesystem::path configured(configured_path);
    const std::array candidates{
        configured,
        std::filesystem::current_path() / configured,
        std::filesystem::path(SAMPIC_DAQ_SOURCE_DIR) / configured,
    };
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_directory(candidate, error)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }
    throw std::runtime_error(
        "calibration directory '" + std::string(configured_path) +
        "' does not exist relative to the working directory or project root");
}

SAMPIC256CH_ErrCode CalibrationLoader::load(
    CrateInfoStruct& info,
    CrateParamStruct& params,
    std::string_view configured_path) {
    const auto directory = resolveDirectory(configured_path);
    const auto text = directory.string();
    if (text.size() >= MAX_PATHNAME_LENGTH) {
        throw std::length_error("resolved calibration directory exceeds vendor path limit");
    }

    std::array<char, MAX_PATHNAME_LENGTH> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
    spdlog::info("Loading SAMPIC calibration data from '{}'", text);
    const auto error =
        SAMPIC256CH_LoadAllCalibValuesFromFiles(&info, &params, buffer.data());
    if (error == SAMPIC256CH_AtLeastOneCalibFileNotFound) {
        spdlog::warn(
            "The calibration directory exists, but the vendor library could not "
            "find at least one file matching this crate's board revision, "
            "sampling frequency, ADC width, or channel");
        for (int feb = 0; feb < info.NbOfFeBoards; ++feb) {
            const auto& board = info.CrateBoardsInfo.FeBoardInfo[feb];
            spdlog::warn(
                "Calibration lookup identity: feb={} type=T{} revision={}.{}, "
                "frequency={} MS/s, adc_bits={}",
                feb, board.FeBoardTypeCharFromEEPROM, board.BoardVersion,
                board.BoardSerNum, params.CommonParams.FreqEch,
                params.CommonParams.ADCNbOfBits);
        }
    }
    return error;
}
