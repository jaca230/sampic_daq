#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/manual_trigger_controller.h"
#include "sampic_tests/modes/double_pulse/config.h"

namespace {

using sampic::double_pulse::ConnectionConfig;
using sampic::double_pulse::DoublePulseConfig;
using sampic::double_pulse::ExternalTriggerConfig;
using sampic::double_pulse::ParameterCombination;
using sampic::double_pulse::ReadoutConfig;
using sampic::double_pulse::StartRetryConfig;

struct Options {
  std::string config_path;
  int max_events = 50;
  double max_duration_s = 10.0;
  bool use_software_trigger = false;
  bool skip_lecroy = false;
};

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + std::string(name));
      }
      return std::string(argv[++i]);
    };
    if (arg == "--config") {
      opts.config_path = require_value(arg);
    } else if (arg == "--events") {
      opts.max_events = std::stoi(require_value(arg));
    } else if (arg == "--duration") {
      opts.max_duration_s = std::stod(require_value(arg));
    } else if (arg == "--software-trigger") {
      opts.use_software_trigger = true;
    } else if (arg == "--skip-lecroy") {
      opts.skip_lecroy = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: external_trigger_probe --config <file> [--events N] "
                   "[--duration seconds] [--software-trigger] [--skip-lecroy]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  if (opts.config_path.empty()) {
    throw std::runtime_error("external_trigger_probe requires --config <file>");
  }
  if (opts.max_events <= 0) {
    throw std::runtime_error("--events must be positive");
  }
  return opts;
}

class SimpleSession {
 public:
  SimpleSession(const ConnectionConfig& conn, const ExternalTriggerConfig& trig)
      : conn_opts_(conn), trig_opts_(trig) {
    initialise_connection();
    configure_base();
    allocate_event_memory();
    configure_defaults();
  }

  ~SimpleSession() {
    stop_run();
    if (event_buffer_ || ml_frames_) {
      SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
    }
    if (connected_) {
      SAMPIC256CH_CloseCrateConnection(&info_);
    }
  }

  void set_sampling_rate(int rate_mhz) {
    check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_, rate_mhz,
                                           conn_opts_.use_external_clock),
          "SetSamplingFrequency");
  }

  void enable_channels(int board_index, const std::vector<int>& channels) {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, ALL_CHANNELs, FALSE),
          "DisableBoardChannels");
    for (int ch : channels) {
      check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, ch, TRUE),
            "EnableChannel");
    }
  }

  bool start_run(const StartRetryConfig& retry_cfg) {
    for (int attempt = 1; attempt <= retry_cfg.max_attempts; ++attempt) {
      const auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
      if (err == SAMPIC256CH_Success) {
        run_active_ = true;
        return true;
      }
      const double sleep_seconds =
          retry_cfg.initial_delay_s * std::pow(retry_cfg.backoff, attempt - 1);
      std::this_thread::sleep_for(std::chrono::duration<double>(sleep_seconds));
    }
    return false;
  }

  void stop_run() {
    if (!run_active_) return;
    SAMPIC256CH_StopRun(&info_, &params_);
    run_active_ = false;
  }

  bool read_event(const ReadoutConfig& readout,
                  EventStruct& event,
                  int& hits_out,
                  int& frames_out,
                  int& bytes_out) {
    SAMPIC256CH_PrepareEvent(&info_, &params_);
    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int loop_counter = 0;
    hits_out = 0;
    bytes_out = 0;

    while (err != SAMPIC256CH_Success) {
      err = SAMPIC256CH_ReadEventBuffer(&info_, 0, event_buffer_, ml_frames_, &nframes);
      if (err == SAMPIC256CH_Success) {
        err = SAMPIC256CH_DecodeEvent(&info_, &params_, ml_frames_, &event, nframes, &hits_out);
      }
      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        std::cerr << "Acquisition/Decode error code " << static_cast<int>(err) << "\n";
        return false;
      }
      if (err != SAMPIC256CH_Success) {
        ++loop_counter;
        if ((loop_counter % readout.prepare_interval) == 0) {
          SAMPIC256CH_PrepareEvent(&info_, &params_);
        }
        if (readout.max_loops > 0 && loop_counter > readout.max_loops) {
          std::cerr << "Read loop exceeded max attempts\n";
          return false;
        }
        if (readout.retry_sleep_us > 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(readout.retry_sleep_us));
        }
      }
    }

    frames_out = nframes;
    for (int i = 0; i < nframes; ++i) {
      const int size = ml_frames_[i].data_size;
      if (size > 0) bytes_out += size;
    }
    return true;
  }

 private:
  void initialise_connection() {
    std::memset(&conn_, 0, sizeof(conn_));
    conn_.ConnectionType = UDP_CONNECTION;
    conn_.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(conn_.CtrlIpAddress, sizeof(conn_.CtrlIpAddress), "%s",
                  conn_opts_.ip.c_str());
    conn_.CtrlPort = conn_opts_.port;
    check(SAMPIC256CH_OpenCrateConnection(conn_, &info_), "OpenCrateConnection");
    connected_ = true;
    std::cout << "Connected to crate. FEBs=" << info_.NbOfFeBoards << "\n";
  }

  void configure_base() {
    check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");
    if (conn_opts_.load_calibration) {
      namespace fs = std::filesystem;
      fs::path calib{conn_opts_.calibration_dir};
      if (!calib.is_absolute()) {
        calib = fs::current_path() / calib;
      }
      std::array<char, MAX_PATHNAME_LENGTH> dir{};
      std::snprintf(dir.data(), dir.size(), "%s", calib.string().c_str());
      const auto err =
          SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dir.data());
      if (err != SAMPIC256CH_Success) {
        std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                  << ")\n";
      }
    }
  }

  void allocate_event_memory() {
    check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_),
          "AllocateEventMemory");
  }

  void configure_defaults() {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
          "DisableAllChannels");

    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, ALL_FE_BOARDs,
                                                  ALL_SAMPICs, ALL_CHANNELs,
                                                  SAMPIC_CHANNEL_EXT_TRIGGER_MODE),
          "SetSampicChannelTriggerMode");

    check(SAMPIC256CH_SetSampicTriggerOption(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                             SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption");

    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_, trig_opts_.trigger_type),
          "SetExternalTriggerType");
    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, trig_opts_.trigger_edge),
          "SetExternalTriggerEdge");
    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, trig_opts_.trigger_level),
          "SetExternalTriggerSigLevel");
    check(SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, trig_opts_.sync_edge),
          "SetExternalSyncEdge");
    check(SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, trig_opts_.sync_level),
          "SetExternalSyncSigLevel");

    check(SAMPIC256CH_SetSampicChannelInternalThreshold(
              &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
              static_cast<float>(conn_opts_.threshold_volts)),
          "SetSampicChannelInternalThreshold");
  }

  void check(SAMPIC256CH_ErrCode err, std::string_view what) {
    if (err != SAMPIC256CH_Success) {
      throw std::runtime_error(std::string(what) + " failed (code " +
                               std::to_string(static_cast<int>(err)) + ")");
    }
  }

  ConnectionConfig conn_opts_;
  ExternalTriggerConfig trig_opts_;
  CrateConnectionParamStruct conn_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool run_active_ = false;
  bool connected_ = false;
};

void print_event(int index, const EventStruct& event, int hits, int frames, int bytes) {
  std::cout << "Event " << index << ": hits=" << hits << " frames=" << frames
            << " bytes=" << bytes << "\n";
  const int capped_hits = std::min(hits, MAX_EXPECTED_FRAMES);
  for (int i = 0; i < capped_hits; ++i) {
    const auto& hit = event.Hit[i];
    std::cout << "    hit[" << i << "]: FEB=" << hit.FeBoardIndex
              << " sampic=" << hit.SampicIndex
              << " channel=" << hit.Channel
              << " first_cell_ts(ns)=" << std::scientific << hit.FirstCellTimeStamp
              << " amplitude=" << hit.Amplitude
              << " tot(ns)=" << hit.TOTValue << std::defaultfloat << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);
    auto cfg = sampic::double_pulse::load_double_pulse_config(opts.config_path);
    if (opts.use_software_trigger) {
      cfg.external_trigger.trigger_type = SOFTWARE;
      std::cout << "Forcing software trigger mode for diagnostics.\n";
    }
    SimpleSession session(cfg.connection, cfg.external_trigger);
    const int rate_mhz =
        cfg.scan.digitizer_rates_mhz.empty() ? 6400 : cfg.scan.digitizer_rates_mhz.front();
    session.set_sampling_rate(rate_mhz);
    session.enable_channels(cfg.scan.board_index, cfg.scan.channels);

    std::unique_ptr<sampic::lecroy::LecroyClient> lecroy;
    std::unique_ptr<sampic::lecroy::ManualTriggerController> manual_trigger;
    if (!opts.skip_lecroy) {
      lecroy = std::make_unique<sampic::lecroy::LecroyClient>();
      lecroy->Configure(cfg.lecroy);
      if (cfg.lecroy.manual_trigger && cfg.lecroy.manual_trigger_interval_s > 0.0) {
        manual_trigger = std::make_unique<sampic::lecroy::ManualTriggerController>(
            lecroy.get(), cfg.lecroy.manual_trigger_interval_s, nullptr);
      }
    } else {
      std::cout << "Skipping Lecroy configuration per user request.\n";
    }

    if (!session.start_run(cfg.start_retry)) {
      throw std::runtime_error("Failed to start run after retry attempts");
    }
    sampic::lecroy::ManualTriggerGuard guard(manual_trigger.get());

    EventStruct event{};
    const auto t_begin = std::chrono::steady_clock::now();
    int events_printed = 0;
    while (events_printed < opts.max_events) {
      const auto now = std::chrono::steady_clock::now();
      if (opts.max_duration_s > 0.0) {
        const double elapsed = std::chrono::duration<double>(now - t_begin).count();
        if (elapsed >= opts.max_duration_s) break;
      }

      int hits = 0;
      int frames = 0;
      int bytes = 0;
      if (!session.read_event(cfg.readout, event, hits, frames, bytes)) {
        continue;
      }
      ++events_printed;
      print_event(events_printed, event, hits, frames, bytes);
    }
    session.stop_run();
    std::cout << "Captured " << events_printed << " events.\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "external_trigger_probe error: " << ex.what() << "\n";
    return 1;
  }
}
