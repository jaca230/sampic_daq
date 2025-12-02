#include "sampic_tests/modes/crate_smoke/crate_smoke_mode.h"

#include <array>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

using ModeOptions = sampic::crate_smoke::CrateSmokeOptions;

std::string describe_error(SAMPIC256CH_ErrCode code) {
  switch (code) {
    case SAMPIC256CH_Success:
      return "Success";
    case SAMPIC256CH_CommError:
      return "Communication error (crate did not respond)";
    case SAMPIC256CH_GenericError:
      return "Generic error (see crate logs)";
    case SAMPIC256CH_CrateNotFound:
      return "Crate not found at specified address";
    case SAMPIC256CH_ConnectionAlreadyOpen:
      return "Connection already open";
    case SAMPIC256CH_NoCrateConnected:
      return "No crate connection established";
    case SAMPIC256CH_NoFeBoardInCrate:
      return "Crate reports zero front-end boards";
    case SAMPIC256CH_OpenDeviceError:
      return "Failed to open USB/PCI device";
    case SAMPIC256CH_InvalidParam:
      return "Invalid parameter";
    case SAMPIC256CH_NoEvent:
      return "No event available";
    case SAMPIC256CH_AcquisitionRunning:
      return "Acquisition already running";
    case SAMPIC256CH_OutOfRange:
      return "Argument out of range";
    case SAMPIC256CH_OutOfMemory:
      return "Out of memory";
    case SAMPIC256CH_AcquisitionError:
      return "Acquisition error during read";
    case SAMPIC256CH_ErrInvalidEvent:
      return "Event buffer contents invalid";
    case SAMPIC256CH_EventNotAllocated:
      return "Event memory not allocated";
    case SAMPIC256CH_SaveDataFileNotOpened:
      return "Unable to open output file";
    case SAMPIC256CH_NoFileFound:
      return "Calibration/data file not found";
    case SAMPIC256CH_ErrorWhileOpeningFile:
      return "Error while opening file";
    case SAMPIC256CH_PurgeBufferIncomplete:
      return "Hardware buffer purge incomplete";
    case SAMPIC256CH_ReachedEndOfFile:
      return "Reached end of replay file";
    case SAMPIC256CH_ReceivedInterruptFrame:
      return "Received interrupt frame";
    case SAMPIC256CH_InconsistencyInBoardsType:
      return "Crate boards report inconsistent types";
    case SAMPIC256CH_Timeout:
      return "Operation timed out";
    case SAMPIC256CH_InvalidConnectionHandle:
      return "Invalid connection handle";
    case SAMPIC256CH_InvalidChannelNumber:
      return "Channel index outside valid range";
    case SAMPIC256CH_FunctionNotAllowed:
      return "Function not allowed in current mode";
    case SAMPIC256CH_CommunicationWriteError:
      return "Communication write error";
    case SAMPIC256CH_CommunicationReadError:
      return "Communication read error";
    case SAMPIC256CH_CommunicationReadRequestError:
      return "Read request failed";
    case SAMPIC256CH_CommunicationReadExtendedError:
      return "Extended read failed";
    case SAMPIC256CH_SAMPICSlowControlAccessError:
      return "Slow-control access error";
    case SAMPIC256CH_ErrNonAllocatedMemory:
      return "Buffer pointer not allocated";
    case SAMPIC256CH_AtLeastOneCalibFileNotFound:
      return "Calibration file missing";
    case SAMPIC256CH_ADCLinearityCalibValuesNotLoaded:
      return "ADC linearity calibration not loaded";
    case SAMPIC256CH_InitDeviceError:
      return "Device initialisation error";
    case SAMPIC256CH_ErrInvalidTriggerDataEvent:
      return "Invalid trigger data";
    case SAMPIC256CH_InvalidHandle:
      return "Invalid handle";
    case SAMPIC256CH_NoFrameRead:
      return "No frame available yet";
    case SAMPIC256CH_NotYetImplemented:
      return "Function not implemented";
    default:
      return "Unknown error";
  }
}

struct CallResult {
  SAMPIC256CH_ErrCode code = SAMPIC256CH_GenericError;
  bool succeeded() const { return code == SAMPIC256CH_Success; }
  std::string message() const {
    std::ostringstream oss;
    oss << "code=" << static_cast<int>(code) << " (" << describe_error(code) << ")";
    return oss.str();
  }
};

template <typename F>
CallResult call_checked(std::string_view label, F&& fn, bool fatal) {
  CallResult result{fn()};
  const auto status = result.succeeded() ? "ok" : "FAIL";
  std::cout << std::left << std::setw(28) << label << " : "
            << status << " - " << result.message() << "\n";
  if (!result.succeeded() && fatal) {
    std::cout << "Aborting after failure in '" << label << "'.\n";
  }
  return result;
}

int perform_smoke_test(const ModeOptions& opts) {
  CrateConnectionParamStruct conn{};
  CrateInfoStruct info{};
  CrateParamStruct params{};
  void* event_buffer = nullptr;
  ML_Frame* ml_frames = nullptr;
  bool connection_open = false;

  auto cleanup = [&] {
    if (event_buffer || ml_frames) {
      SAMPIC256CH_FreeEventMemory(&event_buffer, &ml_frames);
    }
    if (connection_open) {
      SAMPIC256CH_CloseCrateConnection(&info);
    }
  };

  auto guard = [&]() { cleanup(); };

  conn.ConnectionType = UDP_CONNECTION;
  conn.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(conn.CtrlIpAddress, sizeof(conn.CtrlIpAddress), "%s", opts.ip.c_str());
  conn.CtrlPort = opts.port;

  auto res = call_checked("OpenCrateConnection",
                          [&]() {
                            const auto code = SAMPIC256CH_OpenCrateConnection(conn, &info);
                            if (code == SAMPIC256CH_Success) connection_open = true;
                            return code;
                          },
                          true);
  if (!res.succeeded()) {
    guard();
    return 1;
  }

  res = call_checked("SetDefaultParameters",
                     [&]() { return SAMPIC256CH_SetDefaultParameters(&info, &params); }, true);
  if (!res.succeeded()) {
    guard();
    return 1;
  }

  if (opts.load_calibration) {
    namespace fs = std::filesystem;
    fs::path calib{opts.calibration_dir};
    if (!calib.is_absolute()) {
      calib = fs::current_path() / calib;
    }
    std::array<char, MAX_PATHNAME_LENGTH> dir{};
    std::snprintf(dir.data(), dir.size(), "%s", calib.string().c_str());
    res = call_checked("LoadAllCalibValues",
                       [&]() {
                         return SAMPIC256CH_LoadAllCalibValuesFromFiles(&info, &params, dir.data());
                       },
                       false);
  } else {
    std::cout << "Skipping calibration load per user request.\n";
  }

  res = call_checked("AllocateEventMemory",
                     [&]() { return SAMPIC256CH_AllocateEventMemory(&event_buffer, &ml_frames); },
                     true);
  if (!res.succeeded()) {
    guard();
    return 1;
  }

  auto start_res = call_checked("StartRun",
                                [&]() { return SAMPIC256CH_StartRun(&info, &params, TRUE); },
                                true);
  if (!start_res.succeeded()) {
    guard();
    return 1;
  }

  auto stop_run = [&]() { SAMPIC256CH_StopRun(&info, &params); };

  auto attempt_read = [&]() -> CallResult {
    EventStruct event{};
    for (int attempt = 0; attempt < opts.read_attempts; ++attempt) {
      SAMPIC256CH_PrepareEvent(&info, &params);
      int nframes = 0;
      auto read = SAMPIC256CH_ReadEventBuffer(&info, 0, event_buffer, ml_frames, &nframes);
      if (read == SAMPIC256CH_Success) {
        int hits = 0;
        auto decode =
            SAMPIC256CH_DecodeEvent(&info, &params, ml_frames, &event, nframes, &hits);
        return CallResult{decode};
      }
      if (opts.retry_sleep_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(opts.retry_sleep_us));
      }
    }
    return CallResult{SAMPIC256CH_NoFrameRead};
  };

  auto read_res = call_checked("ReadEventBuffer", attempt_read, false);
  stop_run();
  guard();
  return read_res.succeeded() ? 0 : 1;
}

}  // namespace

namespace sampic::crate_smoke {

CrateSmokeMode::CrateSmokeMode(volatile std::sig_atomic_t* stop_flag)
    : stop_flag_(stop_flag) {
  (void)stop_flag_;
}

std::string CrateSmokeMode::name() const {
  return "crate-smoke";
}

std::string CrateSmokeMode::description() const {
  return "Run the crate connectivity smoke test";
}

int CrateSmokeMode::run(int argc, char** argv) {
  const auto opts = parse_args(argc, argv);
  return perform_smoke_test(opts);
}

CrateSmokeOptions CrateSmokeMode::parse_args(int argc, char** argv) {
  CrateSmokeOptions opts;
  for (int i = 0; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + std::string(name));
      }
      return std::string(argv[++i]);
    };

    if (arg == "--ip") {
      opts.ip = require_value(arg);
    } else if (arg == "--port") {
      opts.port = std::stoi(require_value(arg));
    } else if (arg == "--calibration-dir") {
      opts.calibration_dir = require_value(arg);
    } else if (arg == "--no-calibration") {
      opts.load_calibration = false;
    } else if (arg == "--attempts") {
      opts.read_attempts = std::stoi(require_value(arg));
    } else if (arg == "--retry-us") {
      opts.retry_sleep_us = std::stoi(require_value(arg));
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Crate smoke options:\n"
                << "  --ip <addr>              Crate control IP (default 192.168.0.4)\n"
                << "  --port <port>            Control port (default 27015)\n"
                << "  --calibration-dir <dir>  Calibration directory (default resources/calib)\n"
                << "  --no-calibration         Skip calibration load\n"
                << "  --attempts <n>           Max read attempts before giving up (default 25)\n"
                << "  --retry-us <µs>          Sleep between read retries (default 2000)\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown crate-smoke option: " + std::string(arg));
    }
  }
  return opts;
}

}  // namespace sampic::crate_smoke
