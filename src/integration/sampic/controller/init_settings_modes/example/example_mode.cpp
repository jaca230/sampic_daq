#include "integration/sampic/controller/init_settings_modes/example/example_mode.h"
#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/calibration/calibration_loader.h"
#include <spdlog/spdlog.h>
#include <cstring>

SAMPIC_REGISTER_MODE(
    SampicInitSettingsModeRegistry,
    SampicInitSettingsModeExample,
    SampicInitSettingsModeExampleConfig,
    "example",
    "Example initialization",
    [](const SampicInitSettingsModeExampleConfig& config) {
        if (config.ip_address.empty() || config.port <= 0 ||
            config.port > 65535) {
            throw std::invalid_argument(
                "IP address and a valid port are required");
        }
        if (config.connection_type < -1 || config.connection_type > 1 ||
            config.control_type < 0 || config.control_type > 1) {
            throw std::invalid_argument(
                "connection/control type is outside the vendor enum");
        }
    });

int SampicInitSettingsModeExample::initialize() {
    spdlog::info("InitSettingsModeExample: Initializing SAMPIC system...");

    CrateConnectionParamStruct conn{};
    conn.ConnectionType = static_cast<ConnectionType_t>(config_.connection_type);
    conn.ControlBoardControlType = static_cast<ControlType_t>(config_.control_type);
    strncpy(conn.CtrlIpAddress, config_.ip_address.c_str(), sizeof(conn.CtrlIpAddress) - 1);
    conn.CtrlIpAddress[sizeof(conn.CtrlIpAddress) - 1] = '\0';
    conn.CtrlPort = config_.port;

    auto err = SAMPIC256CH_OpenCrateConnection(conn, &info_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeExample: Failed to open crate connection (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeExample: Connection opened with {} FE boards.", info_.NbOfFeBoards);

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeExample: Failed to set default parameters (err={})", static_cast<int>(err));
        return err;
    }

    err = CalibrationLoader::load(
        info_, params_, config_.calibration_directory);
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("InitSettingsModeExample: Calibration files missing, continuing anyway...");
    }

    err = SAMPIC256CH_AllocateEventMemory(&eventBuffer_, &mlFrames_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeExample: Failed to allocate event memory (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeExample: Event memory allocated successfully.");

    return SAMPIC256CH_Success;
}
