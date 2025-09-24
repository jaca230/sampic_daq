#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_default.h"
#include <spdlog/spdlog.h>
#include <cstring>

int SampicInitSettingsModeDefault::initialize() {
    spdlog::info("InitSettingsModeDefault: Initializing SAMPIC system...");

    CrateConnectionParamStruct conn{};
    conn.ConnectionType = settings_.connection_type;
    conn.ControlBoardControlType = settings_.control_type;
    strncpy(conn.CtrlIpAddress, settings_.ip_address.c_str(), sizeof(conn.CtrlIpAddress) - 1);
    conn.CtrlIpAddress[sizeof(conn.CtrlIpAddress) - 1] = '\0';
    conn.CtrlPort = settings_.port;

    auto err = SAMPIC256CH_OpenCrateConnection(conn, &info_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to open crate connection (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeDefault: Connection opened with {} FE boards.", info_.NbOfFeBoards);

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to open crate connection (err={})", static_cast<int>(err));
        return err;
    }

    err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_,
                                                  const_cast<char*>(settings_.calibration_directory.c_str()));
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("InitSettingsModeDefault: Calibration files missing, continuing anyway...");
    }

    err = SAMPIC256CH_AllocateEventMemory(&eventBuffer_, &mlFrames_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to allocate event memory (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeDefault: Event memory allocated successfully.");

    return SAMPIC256CH_Success;
}
