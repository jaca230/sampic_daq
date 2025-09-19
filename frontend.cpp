// ======================================================================
// System Level
// ======================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <unistd.h>

// Midas
#include "midas.h"
#include "mfe.h"

// Sampic
extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

// Project
#include "integration/sampic/sampic_config.h"
#include "integration/midas/frontend_config.h"
#include "integration/midas/odb/odb_manager.h"
#include "integration/midas/odb/odb_utils.h"
#include "integration/spdlog/logger_config.h"
#include "integration/spdlog/logger_configurator.h"

// ======================================================================
// Globals
// ======================================================================
const char *frontend_name = "SAMPIC_Frontend";
const char *frontend_file_name = __FILE__;
BOOL frontend_call_loop = FALSE;
INT display_period = 0;
INT max_event_size = 128 * 1024 * 1024;
INT max_event_size_frag = 5 * max_event_size;
INT event_buffer_size = 5 * max_event_size;
INT frontend_index;
char settings_path[256];

std::mutex settings_mutex;

// SAMPIC globals
CrateConnectionParamStruct CrateConnectionParams;
CrateInfoStruct CrateInfoParams;
CrateParamStruct CrateParams;
void *EventBuffer = nullptr;
ML_Frame *My_ML_Frames = nullptr;
EventStruct Event;
char Message[1024];

bool system_initialized = false;

// Polling
std::chrono::steady_clock::time_point last_poll_time;
std::chrono::microseconds polling_interval(1000000);

// Configs
SampicSystemSettings sampic_cfg;
FrontendConfig fe_config;
LoggerConfig logger_config;

// ======================================================================
// Function declarations
// ======================================================================
INT frontend_init(void);
INT frontend_exit(void);
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop(void);

INT read_sampic_event(char *pevent, INT off);
INT poll_event(INT source, INT count, BOOL test);
INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

// SAMPIC functions
int InitializeSAMPIC(void);
void SetTriggerOptions(void);
int ReadSAMPICEvent(void);
void CleanupSAMPIC(void);

// ======================================================================
// Equipment list
// ======================================================================
BOOL equipment_common_overwrite = TRUE;

EQUIPMENT equipment[] = {
    {"SAMPIC %02d",
        {1, 0,
            "SYSTEM",
            EQ_POLLED | EQ_EB,
            0,
            "MIDAS",
            TRUE,
            RO_RUNNING,
            100, // poll time in ms
            0,
            0,
            TRUE,
            "", "", "",},
        read_sampic_event
    },
    {""}
};

// ======================================================================
// SAMPIC helper functions
// ======================================================================
int InitializeSAMPIC(void) {
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;

    spdlog::info("Initializing SAMPIC system...");

    // Set connection parameters
    CrateConnectionParams.ConnectionType = UDP_CONNECTION;
    CrateConnectionParams.ControlBoardControlType = CTRL_AND_DAQ;
    strncpy(CrateConnectionParams.CtrlIpAddress,
            sampic_cfg.ip_address.c_str(),
            sizeof(CrateConnectionParams.CtrlIpAddress) - 1);
    CrateConnectionParams.CtrlIpAddress[sizeof(CrateConnectionParams.CtrlIpAddress) - 1] = '\0';
    CrateConnectionParams.CtrlPort = sampic_cfg.port;

    // Open connection
    errCode = SAMPIC256CH_OpenCrateConnection(CrateConnectionParams, &CrateInfoParams);

    if (errCode == SAMPIC256CH_Success) {
        spdlog::info("Opened connection with SAMPIC-256Ch Crate ({} FE boards).",
                     CrateInfoParams.NbOfFeBoards);
    } else {
        cm_msg(MERROR, "SAMPIC",
               "No SAMPIC-256Ch Crate found! Error code: %d", errCode);
        return errCode;
    }

    // Load default parameters
    errCode = SAMPIC256CH_SetDefaultParameters(&CrateInfoParams, &CrateParams);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to load default parameters (err=%d)", errCode);
        return errCode;
    }

    // Load calibration files
    errCode = SAMPIC256CH_LoadAllCalibValuesFromFiles(&CrateInfoParams, &CrateParams,
                                                  const_cast<char*>("."));
    if (errCode != SAMPIC256CH_Success) {
        spdlog::warn("At least one calibration file not found, continuing anyway...");
        errCode = SAMPIC256CH_Success;
    }

    // Allocate event memory
    errCode = SAMPIC256CH_AllocateEventMemory(&EventBuffer, &My_ML_Frames);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to allocate event memory (err=%d)", errCode);
        return errCode;
    }
    spdlog::info("Event memory allocated successfully.");

    return errCode;
}

void SetTriggerOptions(void) {
    spdlog::info("Setting trigger options...");
    SAMPIC256CH_ErrCode errCode;

    errCode = SAMPIC256CH_SetChannelMode(&CrateInfoParams, &CrateParams,
                                         ALL_FE_BOARDs, ALL_CHANNELs, TRUE);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to set channel mode (err=%d)", errCode);
        return;
    }

    errCode = SAMPIC256CH_SetSampicChannelTriggerMode(&CrateInfoParams, &CrateParams,
                                                      ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
                                                      SAMPIC_CHANNEL_EXT_TRIGGER_MODE);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to set channel trigger mode (err=%d)", errCode);
        return;
    }

    errCode = SAMPIC256CH_SetSampicTriggerOption(&CrateInfoParams, &CrateParams,
                                                 ALL_FE_BOARDs, ALL_SAMPICs,
                                                 SAMPIC_TRIGGER_IS_L1);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to set trigger option (err=%d)", errCode);
        return;
    }

    errCode = SAMPIC256CH_SetExternalTriggerType(&CrateInfoParams, &CrateParams, SOFTWARE);
    if (errCode != SAMPIC256CH_Success) {
        cm_msg(MERROR, "SAMPIC", "Failed to set external trigger type (err=%d)", errCode);
        return;
    }

    spdlog::info("Trigger options set successfully.");
}

int ReadSAMPICEvent(void) {
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
    int numberOfHits = 0;
    int nframes = 0;
    int nloop_for_soft_trig = 0;
    int dummy = 0;

    SAMPIC256CH_PrepareEvent(&CrateInfoParams, &CrateParams);

    errCode = SAMPIC256CH_NoFrameRead;

    while (errCode != SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_ReadEventBuffer(&CrateInfoParams, dummy,
                                              EventBuffer, My_ML_Frames, &nframes);

        if (errCode == SAMPIC256CH_Success) {
            errCode = SAMPIC256CH_DecodeEvent(&CrateInfoParams, &CrateParams,
                                              My_ML_Frames, &Event, nframes, &numberOfHits);
        }

        if (errCode == SAMPIC256CH_AcquisitionError || errCode == SAMPIC256CH_ErrInvalidEvent) {
            cm_msg(MERROR, "SAMPIC", "Acquisition error (err=%d)", errCode);
            return -1;
        }

        if ((nloop_for_soft_trig % 100) == 0) {
            SAMPIC256CH_PrepareEvent(&CrateInfoParams, &CrateParams);
        }
        nloop_for_soft_trig++;

        if (nloop_for_soft_trig > 10000) {
            spdlog::warn("Timeout waiting for event data");
            return 0;
        }
    }

    if (errCode == SAMPIC256CH_Success) {
        spdlog::debug("Received {} hits in event", numberOfHits);
    }

    return numberOfHits;
}

void CleanupSAMPIC(void) {
    spdlog::info("Cleaning up SAMPIC resources...");
    if (EventBuffer) {
        // SAMPIC256CH_FreeEventMemory(&EventBuffer, &My_ML_Frames);
        EventBuffer = nullptr;
        My_ML_Frames = nullptr;
    }
    // SAMPIC256CH_CloseCrateConnection(&CrateInfoParams);
}

// ======================================================================
// MIDAS Frontend Functions
// ======================================================================
INT frontend_init() {
    frontend_index = get_frontend_index();
    snprintf(settings_path, sizeof(settings_path),
             "/Equipment/SAMPIC %02d/Settings", frontend_index);
    // Set frontend status color → init
    OdbUtils::odbSetStatusColor(frontend_index, fe_config.init_color);

    try {
        OdbManager odb;

        // Paths under this frontend’s settings
        std::string fe_cfg_path     = std::string(settings_path) + "/Frontend";
        std::string logger_cfg_path = std::string(settings_path) + "/Logger";

        odb.initialize(logger_cfg_path, LoggerConfig{});
        logger_config = odb.read<LoggerConfig>(logger_cfg_path);

        // Seed configs
        odb.initialize(settings_path, SampicSystemSettings{});
        odb.initialize(fe_cfg_path, FrontendConfig{});

        // Read configs
        sampic_cfg   = odb.read<SampicSystemSettings>(settings_path);
        fe_config    = odb.read<FrontendConfig>(fe_cfg_path);

        // Apply logger
        LoggerConfigurator::configure(logger_config);

        // Initialize hardware
        int result = InitializeSAMPIC();
        if (result == SAMPIC256CH_Success) {
            system_initialized = true;
            spdlog::info("SAMPIC system initialized successfully");
        } else {
            cm_msg(MERROR, "SAMPIC", "Failed to initialize SAMPIC system (err=%d)", result);
            return FE_ERR_HW;
        }

        // Set frontend status color → init
        OdbUtils::odbSetStatusColor(frontend_index, fe_config.ready_color);
        return SUCCESS;
    } catch (const std::exception& e) {
        cm_msg(MERROR, __FUNCTION__, "Error during frontend_init: %s", e.what());
        return FE_ERR_HW;
    }
}

INT frontend_exit() {
    if (system_initialized) {
        CleanupSAMPIC();
        system_initialized = false;
    }
    return SUCCESS;
}

INT begin_of_run(INT run_number, char *error) {
    if (!system_initialized) {
        strcpy(error, "SAMPIC system not initialized");
        return FE_ERR_HW;
    }

    try {
        OdbManager odb;
        std::string fe_cfg_path     = std::string(settings_path) + "/Frontend";
        std::string logger_cfg_path = std::string(settings_path) + "/Logger";

        // Refresh configs
        sampic_cfg   = odb.read<SampicSystemSettings>(settings_path);
        fe_config    = odb.read<FrontendConfig>(fe_cfg_path);
        logger_config = odb.read<LoggerConfig>(logger_cfg_path);

        polling_interval  = std::chrono::microseconds(fe_config.polling_interval_us);

        // Reconfigure logger if changed
        LoggerConfigurator::configure(logger_config);

    } catch (const std::exception &e) {
        sprintf(error, "Failed to refresh configs at run start: %s", e.what());
        cm_msg(MERROR, __FUNCTION__, "%s", error);
        return FE_ERR_ODB;
    }

    // Apply trigger options and start run
    SetTriggerOptions();
    SAMPIC256CH_ErrCode err = SAMPIC256CH_StartRun(&CrateInfoParams, &CrateParams, TRUE);
    if (err != SAMPIC256CH_Success) {
        sprintf(error, "Error starting SAMPIC run: %d", err);
        cm_msg(MERROR, __FUNCTION__, "%s", error);
        return FE_ERR_HW;
    }

    spdlog::info("Run {} started", run_number);
    return SUCCESS;
}

INT end_of_run(INT run_number, char *error) {
    if (system_initialized) {
        SAMPIC256CH_ErrCode errCode = SAMPIC256CH_StopRun(&CrateInfoParams, &CrateParams);
        if (errCode == SAMPIC256CH_Success) {
            spdlog::info("Run {} stopped", run_number);
        } else {
            sprintf(error, "Error stopping SAMPIC run: %d", errCode);
            cm_msg(MERROR, "SAMPIC", "%s", error);
            return FE_ERR_HW;
        }
    }
    return SUCCESS;
}

INT pause_run(INT, char*) { return SUCCESS; }
INT resume_run(INT, char*) { return SUCCESS; }
INT frontend_loop() { return SUCCESS; }

INT poll_event(INT, INT, BOOL test) {
    auto now = std::chrono::steady_clock::now();
    if (now - last_poll_time >= polling_interval) {
        last_poll_time = now;
        return TRUE;
    }
    return test ? FALSE : 0;
}

INT interrupt_configure(INT, INT, POINTER_T) {
    return SUCCESS;
}

INT read_sampic_event(char *pevent, INT) {
    if (!system_initialized) {
        cm_msg(MERROR, "SAMPIC", "Attempting to read event but system not initialized");
        return 0;
    }

    bk_init32(pevent);

    int numberOfHits = ReadSAMPICEvent();
    if (numberOfHits < 0) {
        cm_msg(MERROR, "SAMPIC", "Error reading SAMPIC event");
        return 0;
    }
    if (numberOfHits == 0) return 0;

    // Bank for SAMPIC data
    char bank_name[8];
    snprintf(bank_name, sizeof(bank_name), "SMP%02d", frontend_index);

    DWORD *pdata;
    bk_create(pevent, bank_name, TID_DWORD, (void **)&pdata);

    *pdata++ = numberOfHits; // store number of hits

    bk_close(pevent, pdata);
    return bk_size(pevent);
}
