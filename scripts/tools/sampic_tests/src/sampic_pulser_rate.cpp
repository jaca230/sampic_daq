extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void signal_handler(int) {
  g_stop_requested = 1;
}

struct Options {
  std::string ip = "192.168.0.4";
  int port = 27015;
  bool load_calibration = true;
  std::string calibration_dir = "/home/pioneer/jcarlton/projects/midas_sampic/experiments/sampic_daq/resources/calib";
  bool pulser_sync = false;
  int pulser_period_ticks = 6400;  // ≈1 µs @ 6.4 GHz
  double threshold = 0.1;          // volts
  int events = 500;                // 0 = unlimited
  double duration_s = 0.0;         // seconds, 0 = unlimited
  int prepare_interval = 100;
  int max_loops = 10000;
  int retry_sleep_us = 100;
  bool quiet = false;
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
    } else if (arg == "--period-ticks") {
      opts.pulser_period_ticks = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--threshold") {
      opts.threshold = std::stod(std::string(require_value(arg)));
    } else if (arg == "--events") {
      opts.events = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--duration") {
      opts.duration_s = std::stod(std::string(require_value(arg)));
    } else if (arg == "--prepare-interval") {
      opts.prepare_interval = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--max-loops") {
      opts.max_loops = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--retry-us") {
      opts.retry_sleep_us = std::stoi(std::string(require_value(arg)));
    } else if (arg == "--no-calibration") {
      opts.load_calibration = false;
    } else if (arg == "--calibration-dir") {
      opts.calibration_dir = std::string(require_value(arg));
    } else if (arg == "--sync-pulser") {
      opts.pulser_sync = true;
    } else if (arg == "--quiet") {
      opts.quiet = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]\n"
                << "  --ip <addr>                 Crate control IP (default 192.168.0.4)\n"
                << "  --port <port>               Control port (default 27015)\n"
                << "  --period-ticks <n>          Pulser period in clock ticks (default 6400)\n"
                << "  --threshold <volts>         Internal threshold (default 0.1 V)\n"
                << "  --events <n>                Stop after N events (0 = unlimited, default 500)\n"
                << "  --duration <seconds>        Stop after duration (0 = unlimited)\n"
                << "  --prepare-interval <n>      Re-send prepare every N read loops (default 100)\n"
                << "  --max-loops <n>             Abort read loop after N retries (default 10000)\n"
                << "  --retry-us <µs>             Sleep between retries (default 100)\n"
                << "  --no-calibration            Skip loading calibration files\n"
                << "  --calibration-dir <path>    Calibration directory (default .)\n"
                << "  --sync-pulser               Enable synchronous pulser mode\n"
                << "  --quiet                     Reduce per-event logging\n"
                << std::endl;
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  return opts;
}

std::string format_duration(std::chrono::steady_clock::duration d) {
  using namespace std::chrono;
  const auto ms = duration_cast<milliseconds>(d);
  const auto sec = ms.count() / 1000.0;
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3) << sec << " s";
  return oss.str();
}

class SampicSession {
 public:
  explicit SampicSession(const Options& opts)
      : opts_(opts) {
    initialise_connection();
    configure_base();
    allocate_event_memory();
  }

  ~SampicSession() {
    if (event_buffer_ || ml_frames_) {
      SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
    }
    if (connected_) {
      SAMPIC256CH_CloseCrateConnection(&info_);
    }
  }

  SampicSession(const SampicSession&) = delete;
  SampicSession& operator=(const SampicSession&) = delete;

  CrateInfoStruct& info() { return info_; }
  CrateParamStruct& params() { return params_; }
  void* event_buffer() { return event_buffer_; }
  ML_Frame* ml_frames() { return ml_frames_; }

  void configure_pulser() {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, TRUE),
          "SetChannelMode");

    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                                 ALL_CHANNELs, SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
          "SetSampicChannelTriggerMode");

    check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                                 ALL_CHANNELs, RISING_EDGE),
          "SetChannelSelflTriggerEdge");

    check(SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                                ALL_CHANNELs, TRUE),
          "SetSampicChannelPulseMode");

    check(SAMPIC256CH_SetSampicChannelInternalThreshold(&info_, &params_, ALL_FE_BOARDs,
                                                        ALL_SAMPICs, ALL_CHANNELs,
                                                        static_cast<float>(opts_.threshold)),
          "SetSampicChannelInternalThreshold");

    check(SAMPIC256CH_SetPulserMode(&info_, &params_, TRUE,
                                    opts_.pulser_sync ? PULSER_SRC_IS_AUTO : PULSER_SRC_IS_AUTO,
                                    opts_.pulser_sync),
          "SetPulserMode");

    check(SAMPIC256CH_SetAutoPulserPeriod(&info_, &params_, opts_.pulser_period_ticks),
          "SetAutoPulserPeriod");
  }

 private:
  void initialise_connection() {
    std::memset(&conn_, 0, sizeof(conn_));
    conn_.ConnectionType = UDP_CONNECTION;
    conn_.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(conn_.CtrlIpAddress, sizeof(conn_.CtrlIpAddress), "%s", opts_.ip.c_str());
    conn_.CtrlPort = opts_.port;

    auto err = SAMPIC256CH_OpenCrateConnection(conn_, &info_);
    check(err, "OpenCrateConnection");
    connected_ = true;
    std::cout << "Connected to crate. Front-end boards: " << info_.NbOfFeBoards << "\n";
  }

  void configure_base() {
    check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");

    if (opts_.load_calibration) {
      namespace fs = std::filesystem;
      fs::path calib_path{opts_.calibration_dir};
      if (!calib_path.is_absolute()) {
        calib_path = fs::current_path() / calib_path;
      }

      std::array<char, MAX_PATHNAME_LENGTH> dir{};
      std::snprintf(dir.data(), dir.size(), "%s", calib_path.string().c_str());
      auto err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dir.data());
      if (err != SAMPIC256CH_Success) {
        std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                  << ")\n";
      } else {
        std::cout << "Calibration loaded from '" << calib_path.string() << "'.\n";
      }
    }
  }

  void allocate_event_memory() {
    check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_),
          "AllocateEventMemory");
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
};

struct AcquisitionStats {
  size_t events = 0;
  size_t total_hits = 0;
  size_t retries = 0;
  size_t decode_errors = 0;
  size_t total_bytes = 0;
  std::chrono::steady_clock::duration elapsed{};
};

AcquisitionStats run_pulser_rate_test(SampicSession& session, const Options& opts) {
  AcquisitionStats stats;
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
    if (g_stop_requested) return true;
    if (opts.events > 0 && static_cast<int>(stats.events) >= opts.events) return true;
    if (opts.duration_s > 0.0) {
      const double elapsed = std::chrono::duration<double>(now - t_begin).count();
      if (elapsed >= opts.duration_s) return true;
    }
    return false;
  };

  while (true) {
    const auto loop_start = std::chrono::steady_clock::now();
    if (should_stop(loop_start)) break;

    SAMPIC256CH_PrepareEvent(&session.info(), &session.params());

    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int hits = 0;
    int loop_counter = 0;

    while (err != SAMPIC256CH_Success) {
      err = SAMPIC256CH_ReadEventBuffer(&session.info(), 0, session.event_buffer(),
                                        session.ml_frames(), &nframes);
      if (err == SAMPIC256CH_Success) {
        err = SAMPIC256CH_DecodeEvent(&session.info(), &session.params(),
                                      session.ml_frames(), &event, nframes, &hits);
      }

      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        ++stats.decode_errors;
        std::cerr << "Acquisition error (code " << static_cast<int>(err) << ")\n";
        break;
      }

      if (err != SAMPIC256CH_Success) {
        ++stats.retries;
        if ((loop_counter % opts.prepare_interval) == 0) {
          SAMPIC256CH_PrepareEvent(&session.info(), &session.params());
        }
        ++loop_counter;
        if (opts.max_loops > 0 && loop_counter > opts.max_loops) {
          std::cerr << "Read loop exceeded max attempts (" << opts.max_loops << ")\n";
          break;
        }
        if (opts.retry_sleep_us > 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(opts.retry_sleep_us));
        }
      }
    }

    if (err == SAMPIC256CH_Success) {
      size_t event_bytes = 0;
      ML_Frame* frames = session.ml_frames();
      for (int i = 0; i < nframes; ++i) {
        const int frame_size = frames[i].data_size;
        if (frame_size > 0) {
          event_bytes += static_cast<size_t>(frame_size);
        }
      }
      stats.total_bytes += event_bytes;
      ++stats.events;
      stats.total_hits += static_cast<size_t>(hits);
      if (!opts.quiet) {
        std::cout << "Event " << stats.events << ": hits=" << hits
                  << " frames=" << nframes
                  << " bytes=" << event_bytes << "\n";
        for (int i = 0; i < hits; ++i) {
          const HitStruct& hit = event.Hit[i];
          std::cout << "    hit[" << i << "]: FEB=" << hit.FeBoardIndex
                    << " sampic=" << hit.SampicIndex
                    << " channel=" << hit.Channel
                    << " first_cell_ts(ns)=" << hit.FirstCellTimeStamp
                    << " amplitude=" << hit.Amplitude
                    << " tot(ns)=" << hit.TOTValue
                    << '\n';
        }
      }
    }
  }

  const auto t_end = std::chrono::steady_clock::now();
  stats.elapsed = t_end - t_begin;
  guard();
  return stats;
}

void print_summary(const AcquisitionStats& stats) {
  const double duration = std::chrono::duration<double>(stats.elapsed).count();
  std::cout << "\nSummary\n-------\n";
  std::cout << "Events       : " << stats.events << "\n";
  std::cout << "Total hits   : " << stats.total_hits << "\n";
  std::cout << "Total bytes  : " << stats.total_bytes << "\n";
  std::cout << "Retries      : " << stats.retries << "\n";
  std::cout << "Decode errors: " << stats.decode_errors << "\n";
  std::cout << "Elapsed      : " << format_duration(stats.elapsed) << "\n";
  if (duration > 0.0) {
    const double bytes_per_second = static_cast<double>(stats.total_bytes) / duration;
    std::cout << std::fixed << std::setprecision(2)
              << "Events/s    : " << stats.events / duration << "\n"
              << "Hits/s      : " << stats.total_hits / duration << "\n"
              << "Data MB/s   : " << bytes_per_second / (1024.0 * 1024.0) << "\n";
    if (stats.events > 0) {
      std::cout << "Hits/event : "
                << static_cast<double>(stats.total_hits) / static_cast<double>(stats.events)
                << "\n";
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const auto opts = parse_args(argc, argv);
    SampicSession session(opts);
    session.configure_pulser();

    const auto stats = run_pulser_rate_test(session, opts);
    print_summary(stats);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}
