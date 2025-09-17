//System Level
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
#include "odbxx.h"
#include "mfe.h"

// Sampic
#ifdef __cplusplus
extern "C" {
#endif

#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>

#ifdef __cplusplus
}
#endif

// Project
#include "config_structs/sampic_config.h"
#include "odb/odb_manager.h"



// Globals
const char *frontend_name = "SAMPIC_Frontend";
const char *frontend_file_name = __FILE__;
BOOL frontend_call_loop = FALSE;
INT display_period = 0;
INT max_event_size = 128 * 1024 * 1024;
INT max_event_size_frag = 5 * max_event_size;
INT event_buffer_size = 5 * max_event_size;
INT frontend_index;
char settings_path[100];

std::mutex settings_mutex;

// SAMPIC globals
CrateConnectionParamStruct CrateConnectionParams;
CrateInfoStruct CrateInfoParams;
CrateParamStruct CrateParams;
void *EventBuffer = NULL;
ML_Frame *My_ML_Frames = NULL;
EventStruct Event;
char Message[1024];

// Settings
bool verbose = false;
int n_events_per_read = 1;
std::string ctrl_ip_address = "192.168.0.4";
int ctrl_port = 27015;
bool system_initialized = false;

//Polling 
std::chrono::steady_clock::time_point last_poll_time;
std::chrono::microseconds polling_interval(1000000);

// Function declarations
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
void Write_InfoMessage(const char *string);
int ReadSAMPICEvent(void);
void CleanupSAMPIC(void);

// Equipment list
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
            100, // poll time in milliseconds
            0,
            0,
            TRUE,
            "", "", "",},
        read_sampic_event
    },
    {""}
};

// ======================================================================
void Write_InfoMessage(const char *string)
// ======================================================================
{
    if (verbose) {
        printf("[SAMPIC Frontend] %s", string);
        cm_msg(MINFO, "SAMPIC", "%s", string);
    }
}

// ======================================================================
int InitializeSAMPIC(void)
// ======================================================================
{
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
    
    Write_InfoMessage("Initializing SAMPIC system...\n");
    
    // Set connection parameters
    CrateConnectionParams.ConnectionType = UDP_CONNECTION;
    CrateConnectionParams.ControlBoardControlType = CTRL_AND_DAQ;
    strncpy(CrateConnectionParams.CtrlIpAddress, ctrl_ip_address.c_str(), sizeof(CrateConnectionParams.CtrlIpAddress) - 1);
    CrateConnectionParams.CtrlIpAddress[sizeof(CrateConnectionParams.CtrlIpAddress) - 1] = '\0';
    CrateConnectionParams.CtrlPort = ctrl_port;
    
    // Open connection
    errCode = SAMPIC256CH_OpenCrateConnection(CrateConnectionParams, &CrateInfoParams);
    
    if (errCode == SAMPIC256CH_Success) {
        Write_InfoMessage("Opened connection with SAMPIC-256Ch Crate.\n");
        sprintf(Message, "Found %d SAMPIC-64Ch Front-End Boards\n", CrateInfoParams.NbOfFeBoards);
        Write_InfoMessage(Message);
    } else {
        cm_msg(MERROR, "SAMPIC", "No SAMPIC-256Ch Crate found! Error code: %d", errCode);
        return errCode;
    }
    
    // Load default parameters
    if (errCode == SAMPIC256CH_Success) {
        Write_InfoMessage("Loading All Hardware Setup parameters...\n");
        errCode = SAMPIC256CH_SetDefaultParameters(&CrateInfoParams, &CrateParams);
        
        if (errCode == SAMPIC256CH_Success) {
            Write_InfoMessage("All Hardware Setup parameters loaded successfully.\n");
        } else {
            if (CrateInfoParams.LastCommErrorInfo.FpgaType == CB_CTRL_FPGA) {
                sprintf(Message, "Error Type %d while accessing 'CTRL_BOARD_CTRL_FPGA' at subadd %d\n", 
                       errCode, CrateInfoParams.LastCommErrorInfo.SubAddress);
            } else if (CrateInfoParams.LastCommErrorInfo.FpgaType == FEB_CTRL_FPGA) {
                sprintf(Message, "Error Type %d while accessing Front-End-Board [%d] 'CTRL_FPGA' at subadd %d\n", 
                       errCode, CrateInfoParams.LastCommErrorInfo.FeBoardTarget, CrateInfoParams.LastCommErrorInfo.SubAddress);
            } else {
                sprintf(Message, "Error Type %d while accessing Front-End-Board [%d] 'FE_FPGA' index %d at subadd %d\n", 
                       errCode, CrateInfoParams.LastCommErrorInfo.FeBoardTarget, 
                       CrateInfoParams.LastCommErrorInfo.FeFpgaTarget, CrateInfoParams.LastCommErrorInfo.SubAddress);
            }
            Write_InfoMessage(Message);
            return errCode;
        }
    }
    
    // Load calibration files
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_LoadAllCalibValuesFromFiles(&CrateInfoParams, &CrateParams, ".");
        if (errCode != SAMPIC256CH_Success) {
            Write_InfoMessage("Warning: At least one calibration file not found!\n");
            // Continue anyway - calibration files may not be critical
            errCode = SAMPIC256CH_Success;
        }
    }
    
    // Allocate event memory
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_AllocateEventMemory(&EventBuffer, &My_ML_Frames);
        if (errCode != SAMPIC256CH_Success) {
            cm_msg(MERROR, "SAMPIC", "Failed to allocate event memory! Error code: %d", errCode);
            return errCode;
        }
        Write_InfoMessage("Event memory allocated successfully.\n");
    }
    
    return errCode;
}

// ======================================================================
void SetTriggerOptions(void)
// ======================================================================
{
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
    
    Write_InfoMessage("Setting trigger options...\n");
    
    // Setting channels ON
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_SetChannelMode(&CrateInfoParams, &CrateParams, ALL_FE_BOARDs, ALL_CHANNELs, TRUE);
        if (errCode != SAMPIC256CH_Success) {
            cm_msg(MERROR, "SAMPIC", "Failed to set channel mode! Error code: %d", errCode);
            return;
        }
    }
    
    // Setting Trigger Parameters
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_SetSampicChannelTriggerMode(&CrateInfoParams, &CrateParams, 
                                                         ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs, 
                                                         SAMPIC_CHANNEL_EXT_TRIGGER_MODE);
        if (errCode != SAMPIC256CH_Success) {
            cm_msg(MERROR, "SAMPIC", "Failed to set channel trigger mode! Error code: %d", errCode);
            return;
        }
    }
    
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_SetSampicTriggerOption(&CrateInfoParams, &CrateParams, 
                                                    ALL_FE_BOARDs, ALL_SAMPICs, SAMPIC_TRIGGER_IS_L1);
        if (errCode != SAMPIC256CH_Success) {
            cm_msg(MERROR, "SAMPIC", "Failed to set trigger option! Error code: %d", errCode);
            return;
        }
    }
    
    if (errCode == SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_SetExternalTriggerType(&CrateInfoParams, &CrateParams, SOFTWARE);
        if (errCode != SAMPIC256CH_Success) {
            cm_msg(MERROR, "SAMPIC", "Failed to set external trigger type! Error code: %d", errCode);
            return;
        }
    }
    
    if (errCode == SAMPIC256CH_Success) {
        Write_InfoMessage("Trigger options set successfully.\n");
    }
}

// ======================================================================
int ReadSAMPICEvent(void)
// ======================================================================
{
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_Success;
    int numberOfHits = 0;
    int nframes = 0;
    int nloop_for_soft_trig = 0;
    int dummy = 0;
    
    // Prepare event (sends software trigger)
    SAMPIC256CH_PrepareEvent(&CrateInfoParams, &CrateParams);
    
    errCode = SAMPIC256CH_NoFrameRead;
    numberOfHits = 0;
    nframes = 0;
    nloop_for_soft_trig = 0;
    
    // Wait for event data
    while (errCode != SAMPIC256CH_Success) {
        errCode = SAMPIC256CH_ReadEventBuffer(&CrateInfoParams, dummy, EventBuffer, My_ML_Frames, &nframes);
        
        if (errCode == SAMPIC256CH_Success) {
            errCode = SAMPIC256CH_DecodeEvent(&CrateInfoParams, &CrateParams, My_ML_Frames, &Event, nframes, &numberOfHits);
        }
        
        if ((errCode == SAMPIC256CH_AcquisitionError) || (errCode == SAMPIC256CH_ErrInvalidEvent)) {
            cm_msg(MERROR, "SAMPIC", "Acquisition error! Error code: %d", errCode);
            return -1;
        }
        
        // Retry software trigger periodically if no data
        if ((nloop_for_soft_trig % 100) == 0) {
            SAMPIC256CH_PrepareEvent(&CrateInfoParams, &CrateParams);
        }
        nloop_for_soft_trig++;
        
        // Add timeout to avoid infinite loop
        if (nloop_for_soft_trig > 10000) {
            cm_msg(MINFO, "SAMPIC", "Timeout waiting for event data");
            return 0; // Return 0 hits rather than error
        }
    }
    
    if (errCode == SAMPIC256CH_Success && verbose) {
        sprintf(Message, "Received %d hits in event\n", numberOfHits);
        Write_InfoMessage(Message);
    }
    
    return numberOfHits;
}

// ======================================================================
void CleanupSAMPIC(void)
// ======================================================================
{
    Write_InfoMessage("Cleaning up SAMPIC resources...\n");
    
    if (EventBuffer) {
        // Note: SAMPIC library should provide cleanup function
        // SAMPIC256CH_FreeEventMemory(&EventBuffer, &My_ML_Frames);
        EventBuffer = NULL;
        My_ML_Frames = NULL;
    }
    
    // Close connection if needed
    // SAMPIC256CH_CloseCrateConnection(&CrateInfoParams);
}

// ======================================================================
// MIDAS Frontend Functions
// ======================================================================

INT frontend_init() {
    frontend_index = get_frontend_index();
    snprintf(settings_path, sizeof(settings_path), "/Equipment/SAMPIC %02d/Settings", frontend_index);

    SampicSystemSettings config;
    OdbManager odbManager;
    odbManager.initialize(settings_path, config);

    // Read initial settings
    std::lock_guard<std::mutex> lock(settings_mutex);
    midas::odb settings;
    settings.connect(settings_path);
    
    verbose = static_cast<bool>(settings["verbose"]);
    n_events_per_read = static_cast<int>(settings["events_per_read"]);
    ctrl_ip_address = static_cast<std::string>(settings["ip_address"]);
    ctrl_port = static_cast<int>(settings["port"]);
    
    // Initialize SAMPIC hardware
    int result = InitializeSAMPIC();
    if (result == SAMPIC256CH_Success) {
        system_initialized = true;
        cm_msg(MINFO, "SAMPIC", "SAMPIC system initialized successfully");
    } else {
        cm_msg(MERROR, "SAMPIC", "Failed to initialize SAMPIC system");
        return FE_ERR_HW;
    }
    
    return SUCCESS;
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
    
    // Read settings from ODB
    std::lock_guard<std::mutex> lock(settings_mutex);
    midas::odb settings;
    settings.connect(settings_path);
    
    verbose = static_cast<bool>(settings["Verbose"]);
    n_events_per_read = static_cast<int>(settings["Events Per Read"]);
    polling_interval = std::chrono::microseconds(static_cast<int>(settings["Polling Interval (us)"]));
    
    // Set trigger options
    SetTriggerOptions();
    
    // Start SAMPIC run
    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_StartRun(&CrateInfoParams, &CrateParams, TRUE);
    if (errCode == SAMPIC256CH_Success) {
        Write_InfoMessage("SAMPIC Run Started.\n");
        cm_msg(MINFO, "SAMPIC", "Run %d started", run_number);
    } else {
        sprintf(error, "Error starting SAMPIC run: %d", errCode);
        cm_msg(MERROR, "SAMPIC", "Error starting run: %d", errCode);
        return FE_ERR_HW;
    }
    
    return SUCCESS;
}

INT end_of_run(INT run_number, char *error) {
    if (system_initialized) {
        SAMPIC256CH_ErrCode errCode = SAMPIC256CH_StopRun(&CrateInfoParams, &CrateParams);
        if (errCode == SAMPIC256CH_Success) {
            Write_InfoMessage("SAMPIC Run Stopped.\n");
            cm_msg(MINFO, "SAMPIC", "Run %d stopped", run_number);
        } else {
            sprintf(error, "Error stopping SAMPIC run: %d", errCode);
            cm_msg(MERROR, "SAMPIC", "Error stopping run: %d", errCode);
            return FE_ERR_HW;
        }
    }
    return SUCCESS;
}

INT pause_run(INT run_number, char *error) {
    return SUCCESS;
}

INT resume_run(INT run_number, char *error) {
    return SUCCESS;
}

INT frontend_loop() {
    return SUCCESS;
}

INT poll_event(INT source, INT count, BOOL test) {
    auto now = std::chrono::steady_clock::now();
    if (now - last_poll_time >= polling_interval) {
        last_poll_time = now;
        return TRUE;
    }
    return test ? FALSE : 0;
}

INT interrupt_configure(INT cmd, INT source, POINTER_T adr) {
    return SUCCESS;
}

INT read_sampic_event(char *pevent, INT off) {
    if (!system_initialized) {
        cm_msg(MERROR, "SAMPIC", "Attempting to read event but system not initialized");
        return 0;
    }
    
    bk_init32(pevent);
    
    // Try to read SAMPIC event
    int numberOfHits = ReadSAMPICEvent();
    
    if (numberOfHits < 0) {
        // Error occurred
        cm_msg(MERROR, "SAMPIC", "Error reading SAMPIC event");
        return 0;
    }
    
    if (numberOfHits == 0) {
        // No data available
        return 0;
    }
    
    // Create bank for SAMPIC data
    char bank_name[8];
    snprintf(bank_name, sizeof(bank_name), "SMP%02d", frontend_index);
    
    // Create bank for hit data
    DWORD *pdata;
    bk_create(pevent, bank_name, TID_DWORD, (void **)&pdata);
    
    // Store number of hits first
    *pdata++ = numberOfHits;
    
    bk_close(pevent, pdata);
    
    return bk_size(pevent);
}