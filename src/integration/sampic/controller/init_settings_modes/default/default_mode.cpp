#include "integration/sampic/controller/init_settings_modes/default/default_mode.h"
#include "core/registry/mode/mode_auto_registration.h"
#include "integration/sampic/calibration/calibration_loader.h"
#include <spdlog/spdlog.h>
#include <cstring>

SAMPIC_REGISTER_MODE(
    SampicInitSettingsModeRegistry,
    SampicInitSettingsModeDefault,
    SampicInitSettingsModeDefaultConfig,
    "default",
    "Standard hardware initialization",
    [](const SampicInitSettingsModeDefaultConfig& config) {
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

int SampicInitSettingsModeDefault::initialize() {
    spdlog::info("InitSettingsModeDefault: Initializing SAMPIC system...");

    CrateConnectionParamStruct conn{};
    conn.ConnectionType = static_cast<ConnectionType_t>(config_.connection_type);
    conn.ControlBoardControlType = static_cast<ControlType_t>(config_.control_type);
    strncpy(conn.CtrlIpAddress, config_.ip_address.c_str(), sizeof(conn.CtrlIpAddress) - 1);
    conn.CtrlIpAddress[sizeof(conn.CtrlIpAddress) - 1] = '\0';
    conn.CtrlPort = config_.port;

    auto err = SAMPIC256CH_OpenCrateConnection(conn, &info_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to open crate connection (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeDefault: Connection opened with {} FE boards.", info_.NbOfFeBoards);

    err = SAMPIC256CH_CheckCrateFirmwareVersions(&info_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to read crate firmware versions (err={})",
                      static_cast<int>(err));
        return err;
    }

    const auto& ctrlFw = info_.CrateBoardsInfo.ControlBoardInfo.FirmwareVersion;
    spdlog::info("InitSettingsModeDefault: Control board firmware v{}.{}.{}",
                 ctrlFw.BoardVersion,
                 ctrlFw.FPGAVersion,
                 ctrlFw.FPGAEvolution);

    for (int feIndex = 0; feIndex < info_.NbOfFeBoards; ++feIndex) {
        const auto& feInfo = info_.CrateBoardsInfo.FeBoardInfo[feIndex];
        const auto& ctrlFpgaFw = feInfo.ControlFpgaFirmwareVersion;
        spdlog::info("InitSettingsModeDefault: FE[{}] control FPGA firmware v{}.{}.{}",
                     feIndex,
                     ctrlFpgaFw.BoardVersion,
                     ctrlFpgaFw.FPGAVersion,
                     ctrlFpgaFw.FPGAEvolution);

        for (int fpgaIndex = 0; fpgaIndex < NB_OF_FE_FPGAS_IN_FE_BOARD; ++fpgaIndex) {
            const auto& feFpgaFw = feInfo.FeFpgaFirmwareVersion[fpgaIndex];
            spdlog::info("InitSettingsModeDefault: FE[{}] FE-FPGA[{}] firmware v{}.{}.{}",
                         feIndex,
                         fpgaIndex,
                         feFpgaFw.BoardVersion,
                         feFpgaFw.FPGAVersion,
                         feFpgaFw.FPGAEvolution);
        }
    }

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to open crate connection (err={})", static_cast<int>(err));
        return err;
    }

    err = CalibrationLoader::load(
        info_, params_, config_.calibration_directory);
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("InitSettingsModeDefault: Calibration files missing, continuing anyway...");
    }
    Boolean adcCorrectionEnabled{};
    Boolean timeInlCorrectionEnabled{};
    Boolean residualPedestalCorrectionEnabled{};
    err = SAMPIC256CH_GetCrateCorrectionLevels(&info_,
                                               &params_,
                                               &adcCorrectionEnabled,
                                               &timeInlCorrectionEnabled,
                                               &residualPedestalCorrectionEnabled);
    if (err != SAMPIC256CH_Success) {
        spdlog::warn("InitSettingsModeDefault: Failed to read crate correction levels (err={})",
                     static_cast<int>(err));
    } else {
        spdlog::info("InitSettingsModeDefault: Correction levels - ADC linearity: {}, Time INL: {}, Residual pedestal: {}",
                      adcCorrectionEnabled ? "enabled" : "disabled",
                      timeInlCorrectionEnabled ? "enabled" : "disabled",
                      residualPedestalCorrectionEnabled ? "enabled" : "disabled");
    }

    err = SAMPIC256CH_AllocateEventMemory(&eventBuffer_, &mlFrames_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("InitSettingsModeDefault: Failed to allocate event memory (err={})", static_cast<int>(err));
        return err;
    }
    spdlog::info("InitSettingsModeDefault: Event memory allocated successfully.");

    return SAMPIC256CH_Success;
}
