#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

struct Options {
  std::string ip = "192.168.0.4";
  int port = 27015;
  int mask = 0xFF;
};

struct FrameInfo {
  int path = -1;
  std::vector<uint8_t> payload;
};

Options parse_args(int argc, char** argv) {
  Options opts;
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
      opts.ip = require("--ip");
    } else if (arg == "--port") {
      opts.port = std::stoi(require("--port"));
    } else if (arg == "--mask") {
      opts.mask = std::stoi(require("--mask"), nullptr, 0);
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: board_probe [--ip addr] [--port port] [--mask 0xFF]\n";
      std::exit(0);
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      std::exit(1);
    }
  }
  return opts;
}

void close_connection(CrateInfoStruct* info) {
  if (info && info->ConnectionInfo.CtrlDeviceHandle >= 0) {
    SAMPIC256CH_CloseCrateConnection(info);
  }
}

SAMPIC256CH_ErrCode probe_mask(CrateInfoStruct* info,
                               uint8_t mask,
                               std::vector<FrameInfo>& frames_out) {
  frames_out.clear();
  const char sub_address = ad_control_board_FeBoardPresence;
  auto err = SAMPIC256CH_BusWriteWords(info, CTRL_ACCESS, CB_CTRL_FPGA, 0, 0, sub_address,
                                       &mask, 1);
  if (err != SAMPIC256CH_Success) return err;
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  err = SAMPIC256CH_BusCommandReadWords(info, CTRL_ACCESS, FEB_CTRL_FPGA, ALL_FE_BOARDs, 0,
                                        0, 1);
  if (err != SAMPIC256CH_Success) return err;

  char temp_buffer[MAX_BYTES_TO_READ]{};
  ML_Frame frames[MAX_EXPECTED_FRAMES];
  int nframes = 0;
  err = SAMPIC256CH_BusReadExtended(info->ConnectionInfo.CtrlDeviceHandle, temp_buffer,
                                    frames, MAX_BYTES_TO_READ, &nframes);
  if (err != SAMPIC256CH_Success) return err;
  for (int idx = 0; idx < nframes; ++idx) {
    FrameInfo info;
    info.path = frames[idx].path[0];
    const auto* data_ptr = reinterpret_cast<unsigned char*>(frames[idx].user_data);
    const int byte_count = frames[idx].data_size;
    if (data_ptr && byte_count > 0) {
      info.payload.assign(data_ptr, data_ptr + byte_count);
    }
    frames_out.push_back(std::move(info));
  }
  return SAMPIC256CH_Success;
}

}  // namespace

struct SlotResult {
  int slot = 0;
  uint8_t mask = 0;
  bool probe_ok = false;
  bool responded = false;
  int path = -1;
  int error = 0;
  std::vector<FrameInfo> frames;
};

std::vector<SlotResult> sweep_slots(CrateInfoStruct* info) {
  std::vector<SlotResult> results;
  for (int slot = 0; slot < MAX_NB_OF_FE_BOARDS; ++slot) {
    SlotResult result;
    result.slot = slot;
    result.mask = static_cast<uint8_t>(1u << slot);
    const auto err = probe_mask(info, result.mask, result.frames);
    if (err != SAMPIC256CH_Success) {
      result.error = static_cast<int>(err);
    } else {
      result.probe_ok = true;
      if (!result.frames.empty() && result.frames.front().path >= 0) {
        result.responded = true;
        result.path = result.frames.front().path;
      }
    }
    results.push_back(result);
  }
  return results;
}

std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) return "<empty>";
  std::ostringstream oss;
  oss << std::hex;
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i) oss << ' ';
    oss.width(2);
    oss.fill('0');
    oss << static_cast<int>(bytes[i]);
  }
  return oss.str();
}

std::string bytes_to_bits(const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) return "<empty>";
  std::ostringstream oss;
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i) oss << ' ';
    for (int bit = 7; bit >= 0; --bit) {
      oss << ((bytes[i] >> bit) & 0x1);
    }
  }
  return oss.str();
}

void explain_slot_result(const SlotResult& res) {
  std::cout << "  Slot " << res.slot << " (mask 0x" << std::hex << static_cast<int>(res.mask)
            << std::dec << "): ";
  if (!res.probe_ok) {
    std::cout << "probe failed (err=" << res.error << ")";
  } else if (!res.responded) {
    std::cout << "no FEB responded.";
  } else {
    std::cout << "FEB present (path index " << res.path << ").";
  }
  if (res.frames.empty()) {
    std::cout << "\n    No frames returned.";
  } else {
    for (size_t idx = 0; idx < res.frames.size(); ++idx) {
      const auto& frame = res.frames[idx];
      std::cout << "\n    Frame[" << idx << "]: path=" << frame.path
                << ", bytes=" << frame.payload.size()
                << ", hex=" << bytes_to_hex(frame.payload)
                << ", bits=" << bytes_to_bits(frame.payload);
    }
  }
  std::cout << "\n";
}

int main(int argc, char** argv) {
  const auto opts = parse_args(argc, argv);
  CrateConnectionParamStruct conn{};
  conn.ConnectionType = UDP_CONNECTION;
  conn.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(conn.CtrlIpAddress, sizeof(conn.CtrlIpAddress), "%s", opts.ip.c_str());
  conn.CtrlPort = opts.port;

  CrateInfoStruct info{};
  auto err = SAMPIC256CH_OpenCrateConnection(conn, &info);
  if (err != SAMPIC256CH_Success) {
    std::cerr << "Failed to open crate connection (err=" << static_cast<int>(err) << ")\n";
    return 1;
  }

  std::cout << "Library reported NbOfFeBoards=" << info.NbOfFeBoards << " during init.\n"
            << "Each slot corresponds to a bit in the control FPGA's FeBoardPresence register:\n"
            << "  bit 0 → slot 0, bit 1 → slot 1, etc. Setting bit N forces the "
            << "crate to poll FEB path N.\n";

  bool broadcast_ok = true;
  std::vector<FrameInfo> detected_frames;
  err = probe_mask(&info, static_cast<uint8_t>(opts.mask), detected_frames);
  if (err != SAMPIC256CH_Success) {
    broadcast_ok = false;
    std::cout << "Broadcast probe with mask 0x" << std::hex << opts.mask << std::dec
              << " failed (err=" << static_cast<int>(err)
              << "). Proceeding with per-slot sweep to gather more info.\n";
  }

  std::vector<int> detected_paths;
  if (broadcast_ok) {
    for (const auto& frame : detected_frames) {
      if (frame.path >= 0) detected_paths.push_back(frame.path);
    }
  }
  std::set<int> unique_paths(detected_paths.begin(), detected_paths.end());
  int broadcast_bitmask = 0;
  for (int path : detected_paths) {
    if (path >= 0 && path < MAX_NB_OF_FE_BOARDS) {
      broadcast_bitmask |= (1 << path);
    }
  }
  if (broadcast_ok) {
    std::cout << "Broadcast probe (mask 0x" << std::hex << opts.mask << std::dec
              << ") frames:";
    if (detected_frames.empty()) {
      std::cout << " <none>";
    } else {
      for (size_t idx = 0; idx < detected_frames.size(); ++idx) {
        const auto& frame = detected_frames[idx];
        std::cout << "\n    Frame[" << idx << "]: path=" << frame.path
                  << ", bytes=" << frame.payload.size()
                  << ", hex=" << bytes_to_hex(frame.payload)
                  << ", bits=" << bytes_to_bits(frame.payload);
      }
    }
    std::cout << "\n  Raw path list:";
    if (detected_paths.empty()) {
      std::cout << " <none>";
    } else {
      for (size_t idx = 0; idx < detected_paths.size(); ++idx) {
        std::cout << " [" << idx << "]=" << detected_paths[idx];
      }
    }
    std::cout << " => aggregated bitmask 0x" << std::hex << broadcast_bitmask << std::dec
              << "\n";
  }
  if (unique_paths.empty()) {
    std::cout << "Broadcast probe returned zero paths; no FEB acknowledged the mask.\n";
  } else {
    std::cout << "Broadcast probe summarized to " << unique_paths.size()
              << " unique path(s):";
    for (int path : unique_paths) std::cout << " " << path;
    std::cout << ".\n";
  }

  std::set<int> individual_paths;
  std::cout << "Sweeping individual presence bits to check each slot...\n";
  const auto slot_results = sweep_slots(&info);
  int individual_bitmask = 0;
  for (const auto& res : slot_results) {
    explain_slot_result(res);
    if (res.responded) {
      individual_paths.insert(res.path);
      individual_bitmask |= (1 << res.path);
    }
  }

  std::cout << "\nInterpretation of slot sweep:\n";
  for (const auto& res : slot_results) {
    std::cout << "  Slot " << res.slot << ": ";
    if (!res.probe_ok) {
      std::cout << "No data (probe error " << res.error << ").\n";
    } else if (!res.responded) {
      std::cout << "No FEB acknowledges this position; bit " << res.slot
                << " is either empty or stuck low.\n";
    } else {
      std::cout << "FEB present; forcing bit " << res.slot << " yields path "
                << res.path << ".\n";
    }
  }

  std::cout << "\nSummary:\n";
  std::cout << "  - Library init saw " << info.NbOfFeBoards << " board(s) based solely on the mask it wrote.\n";
  std::cout << "  - Broadcast probe detected " << unique_paths.size()
            << " path(s) when all bits were asserted at once (bitmask 0x" << std::hex
            << broadcast_bitmask << std::dec << ").\n";
  std::cout << "  - Individual probes detected " << individual_paths.size()
            << " path(s) by forcing each slot separately (bitmask 0x" << std::hex
            << individual_bitmask << std::dec << "):";
  for (int path : individual_paths) std::cout << " " << path;
  std::cout << ".\n";
  if (unique_paths.size() != individual_paths.size()) {
    std::cout << "  -> Mismatch indicates some FEB paths respond only when their bit is driven in isolation; "
              << "the presence mask written during init is not enabling all slots simultaneously.\n";
  } else {
    std::cout << "  -> Broadcast and individual probes agree; detection matches the init count.\n";
  }

  close_connection(&info);
  return 0;
}
