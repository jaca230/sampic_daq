#include "sampic_tests/modes/double_pulse/sampic_session.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace sampic::double_pulse {

namespace {

void check_or_throw(SAMPIC256CH_ErrCode err, const std::string& label) {
  if (err != SAMPIC256CH_Success) {
    throw std::runtime_error(label + " failed (code " +
                             std::to_string(static_cast<int>(err)) + ")");
  }
}

void probe_all_feb_paths(CrateInfoStruct& info) {
  std::set<int> unique_paths;
  uint8_t bitmask = 0;
  for (int slot = 0; slot < MAX_NB_OF_FE_BOARDS; ++slot) {
    const uint8_t mask = static_cast<uint8_t>(1u << slot);
    uint8_t mask_value = mask;
    auto err = SAMPIC256CH_BusWriteWords(&info, CTRL_ACCESS, CB_CTRL_FPGA, 0, 0,
                                         ad_control_board_FeBoardPresence,
                                         &mask_value, 1);
    check_or_throw(err, "ProbeFeBoardPresenceWrite");
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
        bitmask |= static_cast<uint8_t>(1u << path);
      }
    }
  }
  if (unique_paths.empty()) {
    throw std::runtime_error("Presence probe found no responding FEBs");
  }
  uint8_t latched_mask = bitmask;
  auto err = SAMPIC256CH_BusWriteWords(&info, CTRL_ACCESS, CB_CTRL_FPGA, 0, 0,
                                       ad_control_board_FeBoardPresence,
                                       &latched_mask, 1);
  check_or_throw(err, "ProbeFeBoardPresenceLatch");
  info.CrateBoardsInfo.ControlBoardInfo.FeBoardsPresence = latched_mask;
  info.NbOfFeBoards = static_cast<int>(unique_paths.size());
  int idx = 0;
  for (int path : unique_paths) {
    info.FrontEndBoardsPathIndex[idx++] = path;
  }
}

}  // namespace

SampicSession::SampicSession(const ConnectionConfig& conn,
                             const ExternalTriggerConfig& trigger)
    : conn_opts_(conn), trigger_opts_(trigger) {
  initialise_connection();
  configure_base();
  allocate_event_memory();
  configure_channel_defaults();
}

SampicSession::~SampicSession() {
  stop_run();
  if (event_buffer_ || ml_frames_) {
    SAMPIC256CH_FreeEventMemory(&event_buffer_, &ml_frames_);
  }
  if (connected_) {
    SAMPIC256CH_CloseCrateConnection(&info_);
  }
}

void SampicSession::configure_for_combo(const ParameterCombination& combo,
                                        int board_index,
                                        const std::vector<int>& channels) {
  configure_sampling(combo.digitizer_rate_mhz);
  check(SAMPIC256CH_SetAutoConversionMode(&info_, &params_,
                                          combo.auto_conversion ? TRUE : FALSE),
        "SetAutoConversionMode");
  set_threshold(board_index, combo.threshold_volts);
  configure_channel_mask(board_index, channels);
}

void SampicSession::set_threshold(int board_index, double threshold_volts) {
  check(SAMPIC256CH_SetSampicChannelInternalThreshold(
            &info_, &params_, board_index, ALL_SAMPICs, ALL_CHANNELs,
            static_cast<float>(threshold_volts)),
        "SetInternalThreshold");
}

AppliedSettings SampicSession::readback_settings(int board_index) {
  AppliedSettings applied;
  Boolean use_ext = FALSE;
  check(SAMPIC256CH_GetSamplingFrequency(&params_, &applied.sampling_frequency_mhz, &use_ext),
        "GetSamplingFrequency");
  applied.use_external_clock = static_cast<bool>(use_ext);

  if (board_index < 0 || board_index >= info_.NbOfFeBoards) {
    std::ostringstream oss;
    oss << "Board index " << board_index << " out of range (FEB count "
        << info_.NbOfFeBoards << ")";
    throw std::runtime_error(oss.str());
  }

  applied.enabled_channels.clear();
  for (int ch = 0; ch < NB_OF_CHANNELS_IN_FE_BOARD; ++ch) {
    Boolean enabled = FALSE;
    check(SAMPIC256CH_GetChannelMode(&params_, board_index, ch, &enabled),
          "GetChannelMode");
    if (enabled) {
      applied.enabled_channels.push_back(ch);
    }
  }
  return applied;
}

bool SampicSession::start_run_with_retry(const StartRetryConfig& retry_cfg,
                                         int& attempts_out,
                                         std::vector<std::string>& errors) {
  for (int attempt = 1; attempt <= retry_cfg.max_attempts; ++attempt) {
    attempts_out = attempt;
    const auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
    if (err == SAMPIC256CH_Success) {
      run_active_ = true;
      return true;
    }
    std::ostringstream oss;
    oss << "StartRun failed (code=" << static_cast<int>(err)
        << ", attempt=" << attempt << ")";
    errors.push_back(oss.str());
    const double sleep_seconds =
        retry_cfg.initial_delay_s * std::pow(retry_cfg.backoff, attempt - 1);
    std::this_thread::sleep_for(std::chrono::duration<double>(sleep_seconds));
  }
  return false;
}

void SampicSession::stop_run() {
  if (!run_active_) return;
  SAMPIC256CH_StopRun(&info_, &params_);
  run_active_ = false;
}

scan::SampleStats SampicSession::acquire_sample(const ReadoutConfig& readout_cfg,
                                                double duration_s,
                                                volatile std::sig_atomic_t* stop_flag,
                                                bool capture_hits,
                                                std::vector<scan::HitRecord>* hits_out) {
  scan::SampleStats stats;
  EventStruct event{};
  auto last_timestamp_ns = std::optional<double>{};
  constexpr std::size_t kMaxCapturedHits = 512;
  std::size_t captured_hits = 0;
  if (capture_hits && hits_out) {
    hits_out->clear();
  }

  const auto t_begin = std::chrono::steady_clock::now();
  auto should_stop = [&](const std::chrono::steady_clock::time_point& now) {
    if (stop_flag && *stop_flag) return true;
    const double elapsed = std::chrono::duration<double>(now - t_begin).count();
    return elapsed >= duration_s;
  };

  while (!should_stop(std::chrono::steady_clock::now())) {
    SAMPIC256CH_PrepareEvent(&info_, &params_);

    SAMPIC256CH_ErrCode err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int hits = 0;
    int loop_counter = 0;

    while (err != SAMPIC256CH_Success && !(stop_flag && *stop_flag)) {
      err = SAMPIC256CH_ReadEventBuffer(&info_, 0, event_buffer_, ml_frames_, &nframes);
      if (err == SAMPIC256CH_Success) {
        err = SAMPIC256CH_DecodeEvent(&info_, &params_, ml_frames_, &event, nframes, &hits);
      }

      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        ++stats.decode_errors;
        stats.record_error("Acquisition/Decode error code " + std::to_string(static_cast<int>(err)));
        break;
      }

      if (err != SAMPIC256CH_Success) {
        ++stats.retries;
        if ((loop_counter % readout_cfg.prepare_interval) == 0) {
          SAMPIC256CH_PrepareEvent(&info_, &params_);
        }
        ++loop_counter;
        if (readout_cfg.max_loops > 0 && loop_counter > readout_cfg.max_loops) {
          ++stats.max_loop_hits;
          stats.record_error("Read loop exceeded max attempts");
          break;
        }
        if (readout_cfg.retry_sleep_us > 0) {
          std::this_thread::sleep_for(
              std::chrono::microseconds(readout_cfg.retry_sleep_us));
        }
      }
    }

    if (err == SAMPIC256CH_Success) {
      std::size_t event_bytes = 0;
      for (int i = 0; i < nframes; ++i) {
        const int frame_size = ml_frames_[i].data_size;
        if (frame_size > 0) {
          event_bytes += static_cast<std::size_t>(frame_size);
        }
      }
      stats.total_bytes += event_bytes;
      ++stats.events;
      stats.total_hits += static_cast<std::size_t>(hits);
      for (int i = 0; i < hits; ++i) {
        const double timestamp = event.Hit[i].FirstCellTimeStamp;
        stats.channel_hit_counts[{event.Hit[i].FeBoardIndex, event.Hit[i].Channel}] += 1;
        if (last_timestamp_ns.has_value()) {
          const double delta = timestamp - *last_timestamp_ns;
          if (delta >= 0) {
            stats.hit_separation.add(delta);
          }
        }
        last_timestamp_ns = timestamp;
      }

      if (capture_hits && hits_out && captured_hits < kMaxCapturedHits) {
        const int to_copy = std::min(hits, MAX_EXPECTED_FRAMES);
        for (int i = 0; i < to_copy && captured_hits < kMaxCapturedHits; ++i) {
          scan::HitRecord rec;
          rec.board = event.Hit[i].FeBoardIndex;
          rec.sampic = event.Hit[i].SampicIndex;
          rec.channel = event.Hit[i].Channel;
          rec.amplitude = event.Hit[i].Amplitude;
          rec.baseline = event.Hit[i].Baseline;
          rec.tot_ns = event.Hit[i].TOTValue;
          rec.first_cell_ts_ns = event.Hit[i].FirstCellTimeStamp;
          hits_out->push_back(rec);
          ++captured_hits;
        }
      }
    }

    if (stop_flag && *stop_flag) break;
  }

  const auto t_end = std::chrono::steady_clock::now();
  stats.duration_s = std::chrono::duration<double>(t_end - t_begin).count();
  return stats;
}

void SampicSession::initialise_connection() {
  std::memset(&conn_, 0, sizeof(conn_));
  conn_.ConnectionType = UDP_CONNECTION;
  conn_.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(conn_.CtrlIpAddress, sizeof(conn_.CtrlIpAddress), "%s",
                conn_opts_.ip.c_str());
  conn_.CtrlPort = conn_opts_.port;
  check(SAMPIC256CH_OpenCrateConnection(conn_, &info_), "OpenCrateConnection");
  connected_ = true;
  probe_all_feb_paths(info_);
  std::cout << "Connected to crate. FEBs=" << info_.NbOfFeBoards << "\n";
}

void SampicSession::configure_base() {
  check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");
  if (conn_opts_.load_calibration) {
    namespace fs = std::filesystem;
    fs::path calib{conn_opts_.calibration_dir};
    if (!calib.is_absolute()) {
      calib = fs::current_path() / calib;
    }
    std::array<char, MAX_PATHNAME_LENGTH> dir{};
    std::snprintf(dir.data(), dir.size(), "%s", calib.string().c_str());
    const auto err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, dir.data());
    if (err != SAMPIC256CH_Success) {
      std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                << ")\n";
    } else {
      std::cout << "Calibration loaded from '" << calib << "'.\n";
    }
  }
}

void SampicSession::allocate_event_memory() {
  check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &ml_frames_),
        "AllocateEventMemory");
}

void SampicSession::configure_channel_defaults() {
  check(SAMPIC256CH_SetChannelMode(&info_, &params_, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
        "DisableAllChannels");

  if (conn_opts_.use_external_trigger) {
    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, ALL_FE_BOARDs,
                                                  ALL_SAMPICs, ALL_CHANNELs,
                                                  SAMPIC_CHANNEL_EXT_TRIGGER_MODE),
          "SetSampicChannelTriggerMode");
    check(SAMPIC256CH_SetSampicTriggerOption(&info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs,
                                             SAMPIC_TRIGGER_IS_L1),
          "SetSampicTriggerOption");
    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_, trigger_opts_.trigger_type),
          "SetExternalTriggerType");
    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, trigger_opts_.trigger_edge),
          "SetExternalTriggerEdge");
    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, trigger_opts_.trigger_level),
          "SetExternalTriggerSigLevel");
    check(SAMPIC256CH_SetExternalSyncEdge(&info_, &params_, trigger_opts_.sync_edge),
          "SetExternalSyncEdge");
    check(SAMPIC256CH_SetExternalSyncSigLevel(&info_, &params_, trigger_opts_.sync_level),
          "SetExternalSyncSigLevel");
  } else {
    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, ALL_FE_BOARDs,
                                                  ALL_SAMPICs, ALL_CHANNELs,
                                                  SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
          "SetSelfTriggerMode");
    check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_, ALL_FE_BOARDs,
                                                 ALL_SAMPICs, ALL_CHANNELs, RISING_EDGE),
          "SetSelfTriggerEdge");
  }

  check(SAMPIC256CH_SetSampicChannelPulseMode(&info_, &params_, ALL_FE_BOARDs,
                                              ALL_SAMPICs, ALL_CHANNELs, TRUE),
        "SetSampicChannelPulseMode");

  check(SAMPIC256CH_SetSampicChannelInternalThreshold(
            &info_, &params_, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
            static_cast<float>(conn_opts_.threshold_volts)),
        "SetSampicChannelInternalThreshold");
}

void SampicSession::configure_sampling(int rate_mhz) {
  check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_, rate_mhz,
                                         conn_opts_.use_external_clock),
        "SetSamplingFrequency");
}

void SampicSession::configure_channel_mask(int board_index,
                                           const std::vector<int>& channels) {
  if (board_index < 0 || board_index >= info_.NbOfFeBoards) {
    std::ostringstream oss;
    oss << "Board index " << board_index << " out of range (FEB count "
        << info_.NbOfFeBoards << ")";
    throw std::runtime_error(oss.str());
  }
  check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, ALL_CHANNELs, FALSE),
        "DisableBoardChannels");
  for (int channel : channels) {
    check(SAMPIC256CH_SetChannelMode(&info_, &params_, board_index, channel, TRUE),
          "EnableChannel");
  }

  const auto trigger_mode = conn_opts_.use_external_trigger
                                ? SAMPIC_CHANNEL_EXT_TRIGGER_MODE
                                : SAMPIC_CHANNEL_SELF_TRIGGER_MODE;
  check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, board_index,
                                                ALL_SAMPICs, ALL_CHANNELs, trigger_mode),
        "SetBoardTriggerMode");
  if (!conn_opts_.use_external_trigger) {
    check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info_, &params_, board_index,
                                                 ALL_SAMPICs, ALL_CHANNELs, RISING_EDGE),
          "SetBoardSelfTriggerEdge");
  } else {
    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, trigger_opts_.trigger_edge),
          "SetBoardExternalEdge");
  }
}

void SampicSession::check(SAMPIC256CH_ErrCode err, std::string_view what) {
  if (err != SAMPIC256CH_Success) {
    throw std::runtime_error(std::string(what) + " failed (code " +
                             std::to_string(static_cast<int>(err)) + ")");
  }
}

}  // namespace sampic::double_pulse
