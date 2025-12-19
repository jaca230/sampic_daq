#include "sampic_tests/modes/occupancy/occupancy_mode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_crate_configurator.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_default.h"

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
    crate_settings_ = build_settings();
    initialise_system();
  }

  ~OccupancySession() {
    if (event_buffer_ || ml_frames_) {
      SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
    }
    if (connected_) {
      SAMPIC256CH_CloseCrateConnection(&info_);
    }
  }

  void configure_channels() {
    SampicCrateConfigurator configurator(info_, params_, crate_settings_);
    configurator.apply();
  }

  CrateInfoStruct& info() { return info_; }
  CrateParamStruct& params() { return params_; }
  void* event_buffer() { return event_buffer_; }
  ML_Frame* frames() { return ml_frames_; }
  const Options& options() const { return opts_; }

 private:
  void initialise_system() {
    crate_settings_.ip_address = opts_.ip;
    crate_settings_.port = opts_.port;
    if (opts_.load_calibration) {
      if (!opts_.calibration_dir.empty()) {
        crate_settings_.calibration_directory = opts_.calibration_dir;
      }
      SampicControllerConfig controller_cfg;
      SampicInitSettingsModeDefault init_mode(info_, params_, event_buffer_, ml_frames_,
                                              crate_settings_, controller_cfg);
      const int err = init_mode.initialize();
      if (err != SAMPIC256CH_Success) {
        throw std::runtime_error("Failed to initialize SAMPIC system (err=" +
                                 std::to_string(err) + ")");
      }
      connected_ = true;
    } else {
      initialise_without_calibration();
    }
    if (opts_.board_index < 0) {
      auto probe = probe_feb_paths(info_);
      if (!opts_.quiet) {
        std::cout << "Presence probe discovered " << probe.paths.size()
                  << " FEB path(s); mask=0x" << std::hex
                  << static_cast<int>(probe.bitmask) << std::dec << "\n";
        std::cout << "Active FEB paths:";
        for (int path : probe.paths) std::cout << " " << path;
        std::cout << "\n";
      }
    }
    validate_board_index();
    adjust_enabled_boards();
    if (!opts_.quiet) {
      std::cout << "Connected to crate. FEBs=" << info_.NbOfFeBoards << "\n";
    }
  }

  void validate_board_index() {
    if (opts_.board_index < 0) return;
    if (opts_.board_index >= info_.NbOfFeBoards) {
      std::ostringstream oss;
      oss << "Board index " << opts_.board_index << " out of range (FEB count "
          << info_.NbOfFeBoards << ")";
      throw std::runtime_error(oss.str());
    }
  }

  void adjust_enabled_boards() {
    for (auto& [key, feb] : crate_settings_.front_end_boards) {
      const int idx = extract_index(key, "feb");
      bool enable = false;
      if (opts_.board_index >= 0) {
        enable = (idx == opts_.board_index);
      } else {
        enable = (idx < info_.NbOfFeBoards);
      }
      feb.enabled = enable;
      for (auto& [chip_key, chip] : feb.sampics) {
        chip.enabled = true;
        for (auto& [channel_key, channel] : chip.channels) {
          channel.enabled = enable;
        }
      }
    }
  }

  SampicSystemSettings build_settings() {
    SampicSystemSettings settings;
    settings.external_trigger_type = SOFTWARE;
    settings.trigger_edge = RISING_EDGE;
    settings.signal_level = TTL_SIG;
    settings.sync_edge = RISING_EDGE;
    settings.sync_level = TTL_SIG;

    for (auto& [key, feb] : settings.front_end_boards) {
      const int idx = extract_index(key, "feb");
      bool enable_board = false;
      if (opts_.board_index >= 0) {
        enable_board = (idx == opts_.board_index);
      }
      feb.enabled = enable_board;
      for (auto& [chip_key, chip] : feb.sampics) {
        chip.enabled = true;
        for (auto& [channel_key, channel] : chip.channels) {
          channel.enabled = enable_board;
          channel.trigger_mode = SAMPIC_CHANNEL_SELF_TRIGGER_MODE;
          channel.internal_threshold = static_cast<float>(opts_.threshold);
          channel.trigger_edge = RISING_EDGE;
          channel.pulse_mode = true;
          channel.enable_for_central_trigger = true;
        }
      }
    }
    return settings;
  }

  static int extract_index(const std::string& key, const char* prefix) {
    const auto pos = key.find(prefix);
    if (pos == std::string::npos) return 0;
    const auto digits = key.substr(pos + std::strlen(prefix));
    return digits.empty() ? 0 : std::stoi(digits);
  }

  void initialise_without_calibration() {
    CrateConnectionParamStruct conn{};
    conn.ConnectionType = crate_settings_.connection_type;
    conn.ControlBoardControlType = crate_settings_.control_type;
    std::snprintf(conn.CtrlIpAddress, sizeof(conn.CtrlIpAddress), "%s", opts_.ip.c_str());
    conn.CtrlPort = opts_.port;
    auto err = SAMPIC256CH_OpenCrateConnection(conn, &info_);
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error("Failed to open crate connection (err=" + std::to_string(err) +
                               ")");
    }
    connected_ = true;

    err = SAMPIC256CH_CheckCrateFirmwareVersions(&info_);
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error("Failed to read crate firmware versions (err=" +
                               std::to_string(err) + ")");
    }

    err = SAMPIC256CH_SetDefaultParameters(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error("SetDefaultParameters failed (err=" + std::to_string(err) + ")");
    }

    err = SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_);
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error("Failed to allocate event memory (err=" +
                               std::to_string(err) + ")");
    }
    if (!opts_.quiet) {
      std::cout << "Event memory allocated without loading calibration files.\n";
    }
  }

  Options opts_;
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool connected_ = false;
  SampicSystemSettings crate_settings_{};
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
        err = SAMPIC256CH_DecodeEvent(&session.info(), &session.params(),
                                      session.frames(), &event, nframes, &hits);
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
            [](const auto& a, const auto& b) { return a.second > b.second; });

  const double divisor =
      static_cast<double>(summary.events_recorded > 0 ? summary.events_recorded : 1);
  std::cout << "\nChannel hits (sorted by occupancy):\n";
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
                << "  --board <index>          Front-end board index (default 0)\n"
                << "  --events <n>             Stop after N events (default 500)\n"
                << "  --duration <sec>         Stop after duration seconds (0 = unlimited)\n"
                << "  --threshold <volts>      Self-trigger threshold (default 0.1)\n"
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
