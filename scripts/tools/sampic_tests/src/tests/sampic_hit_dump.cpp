#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

CrateConnectionParamStruct g_conn{};
CrateInfoStruct g_info{};
CrateParamStruct g_params{};
void* g_event_buffer = nullptr;
ML_Frame* g_ml_frames = nullptr;
EventStruct g_event{};

std::string g_ip = "192.168.0.4";
int g_port = 27015;
bool g_use_software_trigger = false;
bool g_skip_calibration = false;
std::string g_calib_dir = "resources/calib";
int g_events_to_read = 10;
bool g_connected = false;

void usage(const char* prog) {
  std::cout << "Usage: " << prog << " [options]\n"
            << "  --ip <addr>           Crate IP (default 192.168.0.4)\n"
            << "  --port <port>         Crate port (default 27015)\n"
            << "  --software-trigger    Use software trigger instead of external\n"
            << "  --no-calibration      Skip loading calibration files\n"
            << "  --calibration-dir <d> Calibration directory (default resources/calib)\n"
            << "  --events <n>          Number of events to read (default 10)\n";
}

void Write_InfoMessage(const char* msg) {
  std::cout << msg;
}

void check(SAMPIC256CH_ErrCode code, const char* label) {
  if (code != SAMPIC256CH_Success) {
    std::ostringstream oss;
    oss << label << " failed (code " << static_cast<int>(code) << ")";
    throw std::runtime_error(oss.str());
  }
}

int SystemInit() {
  g_conn.ConnectionType = UDP_CONNECTION;
  g_conn.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(g_conn.CtrlIpAddress, sizeof(g_conn.CtrlIpAddress), "%s", g_ip.c_str());
  g_conn.CtrlPort = g_port;

  auto err = SAMPIC256CH_OpenCrateConnection(g_conn, &g_info);
  if (err != SAMPIC256CH_Success) {
    Write_InfoMessage("No SAMPIC crate found!\n");
    return err;
  }
  g_connected = true;

  Write_InfoMessage("Opened connection with SAMPIC crate.\n");
  std::cout << "Found " << g_info.NbOfFeBoards << " FE boards.\n";

  err = SAMPIC256CH_SetDefaultParameters(&g_info, &g_params);
  if (err != SAMPIC256CH_Success) {
    Write_InfoMessage("Failed to load default parameters.\n");
    return err;
  }

  if (!g_skip_calibration) {
    char dir[MAX_PATHNAME_LENGTH]{};
    std::snprintf(dir, sizeof(dir), "%s", g_calib_dir.c_str());
    const auto calib_err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&g_info, &g_params, dir);
    if (calib_err != SAMPIC256CH_Success) {
      std::cout << "Calibration warning: code " << static_cast<int>(calib_err) << "\n";
    }
  }

  check(SAMPIC256CH_AllocateEventMemory(&g_event_buffer, &g_ml_frames),
        "AllocateEventMemory");
  return SAMPIC256CH_Success;
}

void SetTriggerOptions() {
  check(SAMPIC256CH_SetChannelMode(&g_info, &g_params, ALL_FE_BOARDs, ALL_CHANNELs, TRUE),
        "SetChannelMode");

  const auto mode = g_use_software_trigger ? SAMPIC_CHANNEL_SELF_TRIGGER_MODE
                                           : SAMPIC_CHANNEL_EXT_TRIGGER_MODE;
  check(SAMPIC256CH_SetSampicChannelTriggerMode(&g_info, &g_params, ALL_FE_BOARDs,
                                                ALL_SAMPICs, ALL_CHANNELs, mode),
        "SetSampicChannelTriggerMode");

  if (!g_use_software_trigger) {
    check(SAMPIC256CH_SetSampicTriggerOption(&g_info, &g_params, ALL_FE_BOARDs,
                                             ALL_SAMPICs, SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption");
    check(SAMPIC256CH_SetExternalTriggerType(&g_info, &g_params, EXT_SIG),
          "SetExternalTriggerType");
    check(SAMPIC256CH_SetExternalTriggerEdge(&g_info, &g_params, RISING_EDGE),
          "SetExternalTriggerEdge");
    check(SAMPIC256CH_SetExternalTriggerSigLevel(&g_info, &g_params, TTL_SIG),
          "SetExternalTriggerSigLevel");
  } else {
    //check(SAMPIC256CH_SetExternalTriggerType(&g_info, &g_params, SOFTWARE),
    //      "SetSoftwareTrigger");
  }
}

void PrintHits(int count) {
  for (int i = 0; i < count; ++i) {
    const auto& hit = g_event.Hit[i];
    std::cout << "  hit[" << i << "]: FEB=" << hit.FeBoardIndex
              << " sampic=" << hit.SampicIndex
              << " channel=" << hit.Channel
              << " amplitude=" << hit.Amplitude
              << " baseline=" << hit.Baseline
              << " TOT(ns)=" << hit.TOTValue
              << " first_cell_ts(ns)=" << hit.FirstCellTimeStamp << "\n";
  }
}

void RunAcq() {
  check(SAMPIC256CH_StartRun(&g_info, &g_params, TRUE), "StartRun");
  Write_InfoMessage("Run Started.\n");

  const int n_acq = g_events_to_read;
  for (int loop = 0; loop < n_acq; ++loop) {
    SAMPIC256CH_PrepareEvent(&g_info, &g_params);
    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int hits = 0;
    int soft_loop = 0;

    while (err != SAMPIC256CH_Success) {
      err = SAMPIC256CH_ReadEventBuffer(&g_info, 0, g_event_buffer, g_ml_frames, &nframes);
      if (err == SAMPIC256CH_Success) {
        err = SAMPIC256CH_DecodeEvent(&g_info, &g_params, g_ml_frames, &g_event, nframes, &hits);
      }
      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        Write_InfoMessage("Acquisition error.\n");
        return;
      }
      if ((soft_loop % 100) == 0) {
        SAMPIC256CH_PrepareEvent(&g_info, &g_params);
      }
      ++soft_loop;
    }

    std::cout << "Event " << (loop + 1) << ": hits=" << hits << "\n";
    PrintHits(std::min(hits, MAX_EXPECTED_FRAMES));
  }

  check(SAMPIC256CH_StopRun(&g_info, &g_params), "StopRun");
  Write_InfoMessage("Run Stopped.\n");
}

void Cleanup() {
  if (g_event_buffer || g_ml_frames) {
    SAMPIC256CH_FreeEventMemory(&g_event_buffer, &g_ml_frames);
    g_event_buffer = nullptr;
    g_ml_frames = nullptr;
  }
  if (g_connected) {
    SAMPIC256CH_CloseCrateConnection(&g_info);
    g_connected = false;
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    for (int i = 1; i < argc; ++i) {
      std::string arg{argv[i]};
      auto require = [&](const char* name) -> std::string {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << name << "\n";
          std::exit(1);
        }
        return std::string(argv[++i]);
      };
      if (arg == "--ip") {
        g_ip = require("--ip");
      } else if (arg == "--port") {
        g_port = std::stoi(require("--port"));
      } else if (arg == "--software-trigger") {
        g_use_software_trigger = true;
      } else if (arg == "--no-calibration") {
        g_skip_calibration = true;
      } else if (arg == "--calibration-dir") {
        g_calib_dir = require("--calibration-dir");
      } else if (arg == "--events") {
        g_events_to_read = std::stoi(require("--events"));
      } else if (arg == "--help" || arg == "-h") {
        usage(argv[0]);
        return 0;
      } else {
        std::cerr << "Unknown option: " << arg << "\n";
        usage(argv[0]);
        return 1;
      }
    }

    if (SystemInit() != SAMPIC256CH_Success) {
      Cleanup();
      return 1;
    }
    SetTriggerOptions();
    RunAcq();
    Cleanup();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "sampic_hit_dump error: " << ex.what() << "\n";
    Cleanup();
    return 1;
  }
}
