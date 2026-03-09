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

    SAMPIC256CH_StopRun(&info_, &params_);

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        if (info_.LastCommErrorInfo.FpgaType == CB_CTRL_FPGA) {
            spdlog::error("InitSettingsModeDefault: Error {} while accessing CTRL_BOARD_CTRL_FPGA at subadd {}",
                          static_cast<int>(err),
                          info_.LastCommErrorInfo.SubAddress);
        } else if (info_.LastCommErrorInfo.FpgaType == FEB_CTRL_FPGA) {
            spdlog::error("InitSettingsModeDefault: Error {} while accessing FEB[{}] CTRL_FPGA at subadd {}",
                          static_cast<int>(err),
                          info_.LastCommErrorInfo.FeBoardTarget,
                          info_.LastCommErrorInfo.SubAddress);
        } else {
            spdlog::error("InitSettingsModeDefault: Error {} while accessing FEB[{}] FE_FPGA index {} at subadd {}",
                          static_cast<int>(err),
                          info_.LastCommErrorInfo.FeBoardTarget,
                          info_.LastCommErrorInfo.FeFpgaTarget,
                          info_.LastCommErrorInfo.SubAddress);
        }
        return err;
    }
    spdlog::info("InitSettingsModeDefault: All hardware setup parameters loaded.");

    err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_,
                                                  const_cast<char*>(settings_.calibration_directory.c_str()));
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
