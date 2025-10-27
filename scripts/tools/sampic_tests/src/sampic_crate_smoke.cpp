#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <thread>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

struct Options {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "/home/pioneer/jcarlton/projects/midas_sampic/experiments/sampic_daq/resources/calib";
  int read_attempts = 25;
  int retry_sleep_us = 2000;
};

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + std::string(name));
      }
      return std::string_view{argv[++i]};
    };

    if (arg == "--ip") {
      opts.ip = std::string(require_value(arg));
    } else if (arg == "--port") {
      opts.port = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--calibration-dir") {
      opts.calibration_dir = std::string(require_value(arg));
    } else if (arg == "--no-calibration") {
      opts.load_calibration = false;
    } else if (arg == "--attempts") {
      opts.read_attempts = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--retry-us") {
      opts.retry_sleep_us = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]\n"
                << "  --ip <addr>              Crate control IP (default 192.168.0.4)\n"
                << "  --port <port>            Control port (default 27015)\n"
                << "  --calibration-dir <dir>  Calibration directory (default resources/calib)\n"
                << "  --no-calibration         Skip calibration load\n"
                << "  --attempts <n>           Max read attempts before giving up (default 25)\n"
                << "  --retry-us <µs>          Sleep between read retries (default 2000)\n"
                << std::endl;
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  return opts;
}

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

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);

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

    std::memset(&conn, 0, sizeof(conn));
    conn.ConnectionType = UDP_CONNECTION;
    conn.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(conn.CtrlIpAddress, sizeof(conn.CtrlIpAddress), "%s", opts.ip.c_str());
    conn.CtrlPort = opts.port;

    auto result = call_checked("OpenCrateConnection",
                               [&] { return SAMPIC256CH_OpenCrateConnection(conn, &info); },
                               true);
    if (!result.succeeded()) {
      return 1;
    }
    connection_open = true;

    std::cout << "Front-end boards detected : " << info.NbOfFeBoards << "\n";
    std::cout << "System type               : " << info.SystemType << "\n";
    std::cout << "SAMPIC silicon version    : " << info.SampicVersion << "\n";

    result = call_checked("SetDefaultParameters",
                          [&] { return SAMPIC256CH_SetDefaultParameters(&info, &params); },
                          true);
    if (!result.succeeded()) {
      cleanup();
      return 1;
    }

    if (opts.load_calibration) {
      namespace fs = std::filesystem;
      fs::path calib = opts.calibration_dir;
      if (!calib.is_absolute()) {
        calib = fs::current_path() / calib;
      }
      std::array<char, MAX_PATHNAME_LENGTH> path{};
      std::snprintf(path.data(), path.size(), "%s", calib.string().c_str());
      result = call_checked("LoadAllCalibValuesFromFiles",
                            [&] {
                              return SAMPIC256CH_LoadAllCalibValuesFromFiles(
                                  &info, &params, path.data());
                            },
                            false);
      if (!result.succeeded()) {
        std::cout << "Continuing without calibration data.\n";
      }
    } else {
      std::cout << "Calibration load skipped (per --no-calibration).\n";
    }

    result = call_checked("AllocateEventMemory",
                          [&] { return SAMPIC256CH_AllocateEventMemory(&event_buffer, &ml_frames); },
                          true);
    if (!result.succeeded()) {
      cleanup();
      return 1;
    }

    result = call_checked("StartRun",
                          [&] { return SAMPIC256CH_StartRun(&info, &params, TRUE); },
                          true);
    const bool run_started = result.succeeded();
    if (!run_started) {
      cleanup();
      return 1;
    }

    result = call_checked("PrepareEvent",
                          [&] { return SAMPIC256CH_PrepareEvent(&info, &params); },
                          false);

    EventStruct event{};
    bool decoded = false;
    int final_hits = 0;
    for (int attempt = 1; attempt <= opts.read_attempts; ++attempt) {
      int nframes = 0;
      const auto read_result = SAMPIC256CH_ReadEventBuffer(&info, 0, event_buffer,
                                                           ml_frames, &nframes);
      std::cout << "ReadEventBuffer attempt " << attempt << " : "
                << "code=" << static_cast<int>(read_result)
                << " (" << describe_error(read_result) << ")"
                << ", frames=" << nframes << "\n";

      if (read_result == SAMPIC256CH_Success) {
        int hits = 0;
        auto decode_result = SAMPIC256CH_DecodeEvent(&info, &params, ml_frames, &event,
                                                     nframes, &hits);
        std::cout << "DecodeEvent               : "
                  << "code=" << static_cast<int>(decode_result)
                  << " (" << describe_error(decode_result) << "), hits=" << hits
                  << "\n";
        if (decode_result == SAMPIC256CH_Success) {
          decoded = true;
          final_hits = hits;
        }
        break;
      }

      if (read_result == SAMPIC256CH_NoFrameRead || read_result == SAMPIC256CH_Timeout) {
        SAMPIC256CH_PrepareEvent(&info, &params);
        if (opts.retry_sleep_us > 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(opts.retry_sleep_us));
        }
        continue;
      }

      // Any other error is fatal for the read loop.
      break;
    }

    if (!decoded) {
      std::cout << "No event decoded.\n";
    } else {
      std::cout << "Decoded hits: " << final_hits << "\n";
      for (int i = 0; i < final_hits; ++i) {
        const auto& hit = event.Hit[i];
        std::cout << "  hit[" << i << "]: FEB=" << hit.FeBoardIndex
                  << " sampic=" << hit.SampicIndex
                  << " channel=" << hit.Channel
                  << " first_cell_ts(ns)=" << hit.FirstCellTimeStamp
                  << " amplitude=" << hit.Amplitude
                  << " TOT(ns)=" << hit.TOTValue << "\n";
      }
    }

    call_checked("StopRun", [&] { return SAMPIC256CH_StopRun(&info, &params); }, false);
    cleanup();
    return decoded ? 0 : 2;
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}
