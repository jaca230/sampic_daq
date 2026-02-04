#include "sampic_tests/modes/occupancy/occupancy_mode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

using Options = sampic::occupancy::ChannelOccupancyOptions;

struct ChannelKey {
  int feb = 0;
  int sampic = 0;
  int channel = 0;

  bool operator<(const ChannelKey& other) const {
    if (feb != other.feb) return feb < other.feb;
    if (sampic != other.sampic) return sampic < other.sampic;
    return channel < other.channel;
  }
};

struct AcquisitionSummary {
  int events_requested = 0;
  int events_recorded = 0;
  std::size_t total_hits = 0;
  double duration_s = 0.0;
  std::map<ChannelKey, std::size_t> counts;
};

struct PresenceProbeResult {
  std::vector<int> paths;
  uint8_t bitmask = 0;
};

PresenceProbeResult probe_feb_paths(CrateInfoStruct& info) {
  PresenceProbeResult result;
  std::set<int> unique_paths;
  for (int slot = 0; slot < MAX_NB_OF_FE_BOARDS; ++slot) {
    const uint8_t mask = static_cast<uint8_t>(1u << slot);
    uint8_t mask_value = mask;
    auto err = SAMPIC256CH_BusWriteWords(&info, CTRL_ACCESS, CB_CTRL_FPGA, 0, 0,
                                         ad_control_board_FeBoardPresence,
                                         &mask_value, 1);
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error("Failed to write presence mask for slot " +
                               std::to_string(slot) + " (err=" +
                               std::to_string(err) + ")");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    err = SAMPIC256CH_BusCommandReadWords(&info, CTRL_ACCESS, FEB_CTRL_FPGA,
                                          ALL_FE_BOARDs, 0, 0, 1);
    if (err != SAMPIC256CH_Success) {
      continue;
    }
    char temp_buffer[MAX_BYTES_TO_READ]{};
    ML_Frame frames[MAX_EXPECTED_FRAMES];
    int nframes = 0;
    err = SAMPIC256CH_BusReadExtended(info.ConnectionInfo.CtrlDeviceHandle,
                                      temp_buffer, frames, MAX_BYTES_TO_READ,
                                      &nframes);
    if (err != SAMPIC256CH_Success) {
      continue;
    }
    for (int idx = 0; idx < nframes; ++idx) {
      const int path = frames[idx].path[0];
      if (path >= 0 && path < MAX_NB_OF_FE_BOARDS) {
        unique_paths.insert(path);
        result.bitmask |= static_cast<uint8_t>(1u << path);
      }
    }
  }
  result.paths.assign(unique_paths.begin(), unique_paths.end());
  if (result.paths.empty()) {
    throw std::runtime_error("Presence probe found no responding FEBs");
  }
  uint8_t latched_mask = result.bitmask;
  auto err = SAMPIC256CH_BusWriteWords(&info, CTRL_ACCESS, CB_CTRL_FPGA, 0, 0,
                                       ad_control_board_FeBoardPresence,
                                       &latched_mask, 1);
  if (err != SAMPIC256CH_Success) {
    throw std::runtime_error("Failed to latch aggregated presence mask (err=" +
                             std::to_string(err) + ")");
  }
  info.CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence = latched_mask;
  info.NbOfFeBoards = static_cast<int>(result.paths.size());
  for (int i = 0; i < info.NbOfFeBoards; ++i) {
    info.FrontEndBoardsPathIndex[i] = result.paths[i];
  }
  return result;
}

class OccupancySession {
 public:
  explicit OccupancySession(const Options& opts) : opts_(opts) {
    open_connection();
    set_defaults();
    load_calibration();
    allocate_event_memory();
  }

  ~OccupancySession() {
    if (event_buffer_ || ml_frames_) {
      SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
    }
    if (connected_) {
      SAMPIC256CH_CloseCrateConnection(&info_);
    }
  }

  CrateInfoStruct& info() { return info_; }
  CrateParamStruct& params() { return params_; }
  void* event_buffer() { return event_buffer_; }
  ML_Frame* frames() { return ml_frames_; }

  void configure_channels() {
    enabled_boards_ = select_boards();
    if (enabled_boards_.empty()) {
      throw std::runtime_error("No FEBs selected for occupancy scan");
    }
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
          "DisableAllChannels");
    for (int board : enabled_boards_) {
      check(SAMPIC256CH_SetChannelMode(&info_, &params_, board, ALL_CHANNELs, TRUE),
            "EnableBoardChannels");
      check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, board, ALL_SAMPICs,
                                                    ALL_CHANNELs,
                                                    SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
            "SetTriggerMode");
      check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_, board, ALL_SAMPICs,
                                                   ALL_CHANNELs, RISING_EDGE),
            "SetTriggerEdge");
      check(SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_, board, ALL_SAMPICs,
                                                  ALL_CHANNELs, TRUE),
            "SetPulseMode");
      check(SAMPIC256CH_SetSampicChannelInternalThreshold(
                &info_, &params_, board, ALL_SAMPICs, ALL_CHANNELs,
                static_cast<float>(opts_.threshold)),
            "SetThreshold");
    }
  }

 private:
  void open_connection() {
    std::memset(&conn_, 0, sizeof(conn_));
    conn_.ConnectionType = UDP_CONNECTION;
    conn_.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(conn_.CtrlIpAddress, sizeof(conn_.CtrlIpAddress), "%s",
                  opts_.ip.c_str());
    conn_.CtrlPort = opts_.port;
    check(SAMPIC256CH_OpenCrateConnection(conn_, &info_), "OpenCrateConnection");
    connected_ = true;
    auto probe = probe_feb_paths(info_);
    active_boards_ = probe.paths;
    /*
    std::cout << "Presence probe discovered " << probe.paths.size()
              << " FEB path(s); mask=0x" << std::hex << static_cast<int>(probe.bitmask)
              << std::dec << "\n";
    if (!probe.paths.empty()) {
      std::cout << "Active FEB paths:";
      for (int path : probe.paths) {
        std::cout << " " << path;
      }
      std::cout << "\n";
    }
    std::cout << "Connected to crate. FEBs=" << info_.NbOfFeBoards << "\n";
    */
   }

  void set_defaults() {
    check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");
  }

  void load_calibration() {
    if (!opts_.load_calibration) return;
    namespace fs = std::filesystem;
    fs::path dir{opts_.calibration_dir};
    if (!dir.is_absolute()) {
      dir = fs::current_path() / dir;
    }
    std::array<char, MAX_PATHNAME_LENGTH> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", dir.string().c_str());
    const auto err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, buffer.data());
    if (err != SAMPIC256CH_Success) {
      std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                << ")\n";
    }
  }

  void allocate_event_memory() {
    check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_),
          "AllocateEventMemory");
  }

  std::vector<int> select_boards() const {
    if (opts_.board_index >= 0) {
      auto it = std::find(active_boards_.begin(), active_boards_.end(), opts_.board_index);
      if (it == active_boards_.end()) {
        std::ostringstream oss;
        oss << "Board index " << opts_.board_index << " not present in crate";
        throw std::runtime_error(oss.str());
      }
      return {opts_.board_index};
    }
    return active_boards_;
  }

  void check(SAMPIC256CH_ErrCode err, std::string_view what) {
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error(std::string(what) + " failed (code " +
                               std::to_string(static_cast<int>(err)) + ")");
    }
  }

  Options opts_;
  CrateConnectionParamStruct conn_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool connected_ = false;
  std::vector<int> active_boards_;
  std::vector<int> enabled_boards_;
};

AcquisitionSummary run_occupancy(OccupancySession& session,
                                 const Options& opts,
                                 volatile std::sig_atomic_t* stop_flag) {
  AcquisitionSummary summary;
  summary.events_requested = opts.events;

  EventStruct event{};
  bool run_started = false;
  auto guard = [&]() {
    if (run_started) {
      SAMPIC256CH_StopRun(&session.info(), &session.params());
      run_started = false;
    }
  };

  auto check = [&](SAMPIC256CH_ErrCode err, std::string_view what) {
    if (err != SAMPIC256CH_Success) {
      guard();
      throw std::runtime_error(std::string(what) + " failed (code " +
                               std::to_string(static_cast<int>(err)) + ")");
    }
  };

  check(SAMPIC256CH_StartRun(&session.info(), &session.params(), TRUE), "StartRun");
  run_started = true;
  const auto t_begin = std::chrono::steady_clock::now();

  auto should_stop = [&](const auto& now) {
    if (stop_flag && *stop_flag) return true;
    if (opts.events > 0 && summary.events_recorded >= opts.events) return true;
    if (opts.duration_s > 0.0) {
      const double elapsed = std::chrono::duration<double>(now - t_begin).count();
      if (elapsed >= opts.duration_s) return true;
    }
    return false;
  };

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (should_stop(now)) break;

    SAMPIC256CH_PrepareEvent(&session.info(), &session.params());

    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int hits = 0;
    int loop_counter = 0;

    while (err != SAMPIC256CH_Success) {
      err = SAMPIC256CH_ReadEventBuffer(&session.info(), 0, session.event_buffer(),
                                        session.frames(), &nframes);
      if (err == SAMPIC256CH_Success) {
        if (nframes <= 0) {
          err = SAMPIC256CH_NoFrameRead;
        } else {
          err = SAMPIC256CH_DecodeEvent(&session.info(), &session.params(),
                                        session.frames(), &event, nframes, &hits);
        }
      }
      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        throw std::runtime_error("Acquisition error while reading occupancy data");
      }
      if ((loop_counter % opts.prepare_interval) == 0) {
        SAMPIC256CH_PrepareEvent(&session.info(), &session.params());
      }
      if (loop_counter >= opts.max_loops) {
        throw std::runtime_error("Exceeded read loop retry budget");
      }
      ++loop_counter;
      if (err != SAMPIC256CH_Success) {
        std::this_thread::sleep_for(std::chrono::microseconds(opts.retry_sleep_us));
      }
    }

    summary.events_recorded++;
    summary.total_hits += static_cast<std::size_t>(hits);
    const int capped_hits = std::min(hits, MAX_EXPECTED_FRAMES);
    for (int i = 0; i < capped_hits; ++i) {
      const auto& hit = event.Hit[i];
      ChannelKey key{hit.FeBoardIndex, hit.SampicIndex, hit.Channel};
      summary.counts[key]++;
    }
  }

  guard();
  summary.duration_s =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t_begin).count();
  return summary;
}

void print_summary(const AcquisitionSummary& summary, const Options& opts) {
  std::cout << "Channel occupancy summary\n-------------------------\n";
  std::cout << "Board index   : " << opts.board_index << "\n";
  std::cout << "Events req'd  : " << opts.events << "\n";
  std::cout << "Events read   : " << summary.events_recorded << "\n";
  std::cout << "Total hits    : " << summary.total_hits << "\n";
  std::cout << "Elapsed (s)   : " << std::fixed << std::setprecision(3) << summary.duration_s
            << "\n";

  if (summary.counts.empty()) {
    std::cout << "No hits recorded.\n";
    return;
  }

  std::vector<std::pair<ChannelKey, std::size_t>> entries{summary.counts.begin(),
                                                          summary.counts.end()};
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
              return a.first < b.first;
            });

  const double divisor =
      static_cast<double>(summary.events_recorded > 0 ? summary.events_recorded : 1);
  std::cout << "\nChannel hits (sorted by FEB/SAMPIC/channel):\n";
  for (const auto& [key, hits] : entries) {
    const double per_event = static_cast<double>(hits) / divisor;
    std::cout << "  FEB " << key.feb << " Sampic " << key.sampic << " Ch " << key.channel
              << ": " << hits << " hits (" << std::fixed << std::setprecision(3)
              << per_event << " / event)\n";
  }
}

nlohmann::json summary_to_json(const AcquisitionSummary& summary, const Options& opts) {
  nlohmann::json root{
      {"board_index", opts.board_index},
      {"events_requested", opts.events},
      {"events_recorded", summary.events_recorded},
      {"total_hits", summary.total_hits},
      {"duration_s", summary.duration_s},
  };
  const double divisor =
      static_cast<double>(summary.events_recorded > 0 ? summary.events_recorded : 1);
  nlohmann::json channels = nlohmann::json::array();
  for (const auto& [key, hits] : summary.counts) {
    nlohmann::json entry{
        {"feb", key.feb},
        {"sampic", key.sampic},
        {"channel", key.channel},
        {"hits", hits},
        {"hits_per_event", static_cast<double>(hits) / divisor},
    };
    channels.push_back(entry);
  }
  root["channels"] = std::move(channels);
  return root;
}

}  // namespace

namespace sampic::occupancy {

ChannelOccupancyMode::ChannelOccupancyMode(volatile std::sig_atomic_t* stop_flag)
    : stop_flag_(stop_flag) {}

std::string ChannelOccupancyMode::name() const {
  return "occupancy";
}

std::string ChannelOccupancyMode::description() const {
  return "Self-trigger channel occupancy sampler";
}

int ChannelOccupancyMode::run(int argc, char** argv) {
  const auto opts = parse_args(argc, argv);
  OccupancySession session(opts);
  session.configure_channels();
  const auto summary = run_occupancy(session, opts, stop_flag_);
  if (!opts.quiet) {
    print_summary(summary, opts);
  }
  if (opts.json_output) {
    auto json = summary_to_json(summary, opts);
    std::cout << json.dump(2) << "\n";
  }
  return 0;
}

ChannelOccupancyOptions ChannelOccupancyMode::parse_args(int argc, char** argv) {
  ChannelOccupancyOptions opts;
  for (int i = 0; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + std::string(name));
      }
      return std::string(argv[++i]);
    };

    if (arg == "--ip") {
      opts.ip = require_value(arg);
    } else if (arg == "--port") {
      opts.port = std::stoi(require_value(arg));
    } else if (arg == "--board") {
      opts.board_index = std::stoi(require_value(arg));
    } else if (arg == "--events") {
      opts.events = std::stoi(require_value(arg));
    } else if (arg == "--duration") {
      opts.duration_s = std::stod(require_value(arg));
    } else if (arg == "--threshold") {
      opts.threshold = std::stod(require_value(arg));
    } else if (arg == "--prepare-interval") {
      opts.prepare_interval = std::stoi(require_value(arg));
    } else if (arg == "--max-loops") {
      opts.max_loops = std::stoi(require_value(arg));
    } else if (arg == "--retry-us") {
      opts.retry_sleep_us = std::stoi(require_value(arg));
    } else if (arg == "--calibration-dir") {
      opts.calibration_dir = require_value(arg);
    } else if (arg == "--no-calibration") {
      opts.load_calibration = false;
    } else if (arg == "--json") {
      opts.json_output = true;
    } else if (arg == "--quiet") {
      opts.quiet = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Channel occupancy mode options:\n"
                << "  --ip <addr>              Crate IP (default 192.168.0.4)\n"
                << "  --port <port>            Crate port (default 27015)\n"
                << "  --board <index>          Front-end board index (-1 = all)\n"
                << "  --events <n>             Stop after N events (default 500)\n"
                << "  --duration <sec>         Stop after duration seconds (0 = unlimited)\n"
                << "  --threshold <volts>      Self-trigger threshold (default 0.02)\n"
                << "  --prepare-interval <n>   Re-send prepare after N read loops (default 100)\n"
                << "  --max-loops <n>          Abort read loop after N retries (default 10000)\n"
                << "  --retry-us <µs>          Sleep between retries (default 100)\n"
                << "  --no-calibration         Skip loading calibration files\n"
                << "  --calibration-dir <dir>  Calibration directory\n"
                << "  --json                   Emit JSON summary\n"
                << "  --quiet                  Suppress human-readable summary/logs\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown occupancy option: " + std::string(arg));
    }
  }
  if (opts.events < 0) {
    throw std::runtime_error("--events must be non-negative");
  }
  return opts;
}

}  // namespace sampic::occupancy
