// One supervised, bounded threshold measurement. No per-hit output or event
// files.
extern "C" {
#include <SAMPIC_256Ch_lib.h>
}
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t interrupted = 0;
void signalHandler(int) { interrupted = 1; }
void check(SAMPIC256CH_ErrCode code, const std::string &name) {
  if (code != SAMPIC256CH_Success)
    throw std::runtime_error(name + " failed (code " + std::to_string(code) +
                             ")");
}
int commandAttempts = 3;
double commandRetryDelay = 0.5;
bool transientCommunication(SAMPIC256CH_ErrCode code) {
  return code == SAMPIC256CH_Timeout ||
         code == SAMPIC256CH_CommunicationWriteError ||
         code == SAMPIC256CH_CommunicationReadError ||
         code == SAMPIC256CH_CommunicationReadRequestError ||
         code == SAMPIC256CH_CommunicationReadExtendedError;
}
template <typename Command>
void retryCommand(Command command, const std::string &name) {
  for (int attempt = 1;; ++attempt) {
    const auto code = command();
    if (code == SAMPIC256CH_Success)
      return;
    if (!transientCommunication(code) || attempt >= commandAttempts)
      check(code, name);
    const double delay =
        std::min(commandRetryDelay * std::pow(2.0, attempt - 1), 4.0);
    std::cerr << name << " communication error " << code << "; waiting "
              << delay << "s then retrying on EXISTING connection ("
              << attempt + 1 << "/" << commandAttempts << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::duration<double>(delay));
  }
}
void save(const std::filesystem::path &path, const Json &value) {
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream file(tmp);
    if (!file)
      throw std::runtime_error("Cannot write " + tmp);
    file << value.dump(2) << '\n';
    file.flush();
    if (!file)
      throw std::runtime_error("Write failed: " + tmp);
  }
  std::filesystem::rename(tmp, path);
}
struct Session {
  std::unique_ptr<CrateInfoStruct> info = std::make_unique<CrateInfoStruct>();
  std::unique_ptr<CrateParamStruct> params =
      std::make_unique<CrateParamStruct>();
  void *buffer = nullptr;
  ML_Frame *frames = nullptr;
  bool opened = false, started = false, offset_specific_baseline = false;
  explicit Session(const Json &config) {
    CrateConnectionParamStruct connection{};
    connection.ConnectionType = UDP_CONNECTION;
    connection.ControlBoardControlType = CTRL_AND_DAQ;
    const auto ip = config.at("crate").at("ip").get<std::string>();
    if (ip.size() >= sizeof(connection.CtrlIpAddress))
      throw std::invalid_argument("IP address too long");
    std::memcpy(connection.CtrlIpAddress, ip.c_str(), ip.size() + 1);
    connection.CtrlPort = config.at("crate").at("port").get<int>();
    check(SAMPIC256CH_OpenCrateConnection(connection, info.get()),
          "OpenCrateConnection");
    opened = true;
  }
  ~Session() {
    // The supervisor handles abnormal/failed cleanup with a separate recovery
    // process. Never run an unbounded StopRun from a destructor.
    if (buffer || frames)
      SAMPIC256CH_FreeEventMemory(&buffer, &frames);
    if (opened)
      SAMPIC256CH_CloseCrateConnection(info.get());
  }
  void quiesce() {
    // Match the vendor StopRun ordering: inhibit the control-board trigger
    // before slow per-channel register writes. Retry only idempotent disables.
    SAMPIC256CH_ErrCode trigger = SAMPIC256CH_Success;
    SAMPIC256CH_ErrCode channels = SAMPIC256CH_Success;
    for (int attempt = 1; attempt <= 3; ++attempt) {
      trigger = SAMPIC256CH_SetControlBoardEnableTrigger(info.get(),
                                                         params.get(), FALSE);
      channels = SAMPIC256CH_SetChannelMode(info.get(), params.get(),
                                            ALL_FE_BOARDs, ALL_CHANNELs, FALSE);
      if (trigger == SAMPIC256CH_Success && channels == SAMPIC256CH_Success)
        return;
      std::cerr << "Source-disable attempt " << attempt
                << "/3: trigger=" << trigger << ", channels=" << channels
                << std::endl;
      const auto transient = [](SAMPIC256CH_ErrCode code) {
        return code == SAMPIC256CH_Success || code == SAMPIC256CH_Timeout ||
               code == SAMPIC256CH_CommunicationWriteError ||
               code == SAMPIC256CH_CommunicationReadError ||
               code == SAMPIC256CH_CommunicationReadRequestError ||
               code == SAMPIC256CH_CommunicationReadExtendedError;
      };
      if (!transient(trigger) || !transient(channels))
        break;
      if (attempt < 3) {
        std::cerr
            << "Waiting before retrying source disable on EXISTING connection"
            << std::endl;
        std::this_thread::sleep_for(std::chrono::duration<double>(
            std::min(commandRetryDelay * std::pow(2.0, attempt - 1), 4.0)));
      }
    }
    check(trigger, "DisableControlBoardTrigger");
    check(channels, "DisableAllChannels");
  }

  void configure(const Json &config) {
    retryCommand(
        [&] {
          return SAMPIC256CH_SetDefaultParameters(info.get(), params.get());
        },
        "SetDefaultParameters");
    retryCommand(
        [&] {
          return SAMPIC256CH_SetPulserMode(info.get(), params.get(), FALSE,
                                           PULSER_SRC_IS_AUTO, TRUE);
        },
        "DisableInternalPulser");
    retryCommand(
        [&] {
          return SAMPIC256CH_SetSamplingFrequency(
              info.get(), params.get(),
              config.at("sampling_frequency_mhz").get<int>(), FALSE);
        },
        "SetSamplingFrequency");
    const auto directory =
        config.at("crate").at("calibration_dir").get<std::string>();
    std::array<char, MAX_PATHNAME_LENGTH> path{};
    if (directory.size() >= path.size())
      throw std::invalid_argument("Calibration path too long");
    std::memcpy(path.data(), directory.c_str(), directory.size() + 1);
    retryCommand(
        [&] {
          return SAMPIC256CH_LoadAllCalibValuesFromFiles(
              info.get(), params.get(), path.data());
        },
        "LoadCalibration");
    retryCommand(
        [&] {
          return Set_SystemADCNbOfBits(info.get(), params.get(),
                                       config.at("adc_bits").get<int>());
        },
        "SetADCResolution");
    retryCommand(
        [&] {
          return SAMPIC256CH_SetBaselineReference(
              info.get(), params.get(), ALL_FE_BOARDs, ALL_SAMPICs,
              config.at("baseline_v").get<float>());
        },
        "SetBaseline");
    Boolean specific = FALSE;
    retryCommand(
        [&] {
          return SAMPIC256CH_ReloadInternalTriggerThresholdOffsetsFromFiles(
              info.get(), params.get(), path.data(), &specific);
        },
        "LoadThresholdOffsets");
    offset_specific_baseline = specific != FALSE;
    retryCommand(
        [&] {
          return SAMPIC256CH_SetCrateCorrectionLevels(info.get(), params.get(),
                                                      FALSE, FALSE, FALSE);
        },
        "DisableWaveformCorrections");
    const int samples = config.at("samples_per_hit").get<int>();
    retryCommand(
        [&] {
          return SAMPIC256CH_SetSmartReadMode(info.get(), params.get(),
                                              samples < 64 ? TRUE : FALSE,
                                              samples, 0);
        },
        "SetReadLength");
    retryCommand(
        [&] {
          return SAMPIC256CH_SetNbOfFramesPerBlock(info.get(), params.get(), 1);
        },
        "SetFramesPerBlock");
    retryCommand(
        [&] {
          return SAMPIC256CH_SetExternalTriggerCounterMode(
              info.get(), params.get(), FALSE, FALSE);
        },
        "DisableExternalTriggerRecords");
    check(SAMPIC256CH_AllocateEventMemory(&buffer, &frames),
          "AllocateEventMemory");
  }
  Json measure(const Json &request) {
    const auto &config = request.at("config");
    std::cout
        << "Configuring defaults, calibration, self triggers and channel mask"
        << std::endl;
    configure(config);
    const auto channels =
        request.at("channels").get<std::vector<std::array<int, 2>>>();
    if (channels.empty())
      throw std::invalid_argument("Empty measurement group");
    // A restricted mask (explicit selection or adaptive chunk) must exclude
    // other channels. Full-crate scans do not need a blanket disable write.
    std::array<bool, 256> seen{};
    for (const auto &[feb, channel] : channels) {
      if (feb < 0 || feb >= info->NbOfFeBoards || channel < 0 || channel >= 64)
        throw std::invalid_argument("Unavailable FEB/channel");
      if (seen[feb * 64 + channel])
        throw std::invalid_argument("Duplicate FEB/channel");
      seen[feb * 64 + channel] = true;
    }
    if (channels.size() < static_cast<std::size_t>(info->NbOfFeBoards * 64)) {
      std::cout << "Applying restricted channel mask for selection/chunk"
                << std::endl;
      retryCommand(
          [&] {
            return SAMPIC256CH_SetChannelMode(
                info.get(), params.get(), ALL_FE_BOARDs, ALL_CHANNELs, FALSE);
          },
          "ApplyRestrictedChannelMask");
    }
    const double threshold = request.at("threshold_v").get<double>();
    Json counts = Json::object(), applied = Json::object();
    std::array<bool, 256> selected{};
    for (const auto &[feb, channel] : channels) {
      if (feb < 0 || feb >= info->NbOfFeBoards || channel < 0 || channel >= 64)
        throw std::invalid_argument("Unavailable FEB/channel");
      const auto key = std::to_string(feb) + ":" + std::to_string(channel);
      counts[key] = 0;
      selected[feb * 64 + channel] = true;
      retryCommand(
          [&] {
            return SAMPIC256CH_SetSampicChannelTriggerMode(
                info.get(), params.get(), feb, channel / 16, channel % 16,
                SAMPIC_CHANNEL_SELF_TRIGGER_MODE);
          },
          "SetSelfTrigger");
      retryCommand(
          [&] {
            return SAMPIC256CH_SetChannelSelflTriggerEdge(
                info.get(), params.get(), feb, channel / 16, channel % 16,
                config.at("edge") == "rising" ? RISING_EDGE : FALLING_EDGE);
          },
          "SetTriggerEdge");
      retryCommand(
          [&] {
            return SAMPIC256CH_SetSampicChannelPulseMode(
                info.get(), params.get(), feb, channel / 16, channel % 16,
                TRUE);
          },
          "SetPulseMode");
      retryCommand(
          [&] {
            return SAMPIC256CH_SetSampicChannelInternalThreshold(
                info.get(), params.get(), feb, channel / 16, channel % 16,
                static_cast<float>(threshold));
          },
          "SetThreshold");
      float value = 0;
      check(SAMPIC256CH_GetSampicChannelInternalThreshold(
                params.get(), feb, channel / 16, channel % 16, &value),
            "GetThreshold");
      if (std::abs(value - threshold) > 1e-6)
        throw std::runtime_error(
            "Requested threshold clamped by baseline/DAC range");
      applied[key] = value;
      retryCommand(
          [&] {
            return SAMPIC256CH_SetChannelMode(info.get(), params.get(), feb,
                                              channel, TRUE);
          },
          "EnableChannel");
    }
    auto event = std::make_unique<EventStruct>();
    check(SAMPIC256CH_StartRun(info.get(), params.get(), TRUE), "StartRun");
    started = true;
    std::cout << "Acquisition started" << std::endl;
    const auto begin = Clock::now();
    std::uint64_t hits = 0, bytes = 0, events = 0;
    std::string status = "complete";
    while (true) {
      const double elapsed =
          std::chrono::duration<double>(Clock::now() - begin).count();
      if (interrupted) {
        status = "interrupted";
        break;
      }
      if (elapsed >= config.at("duration_s").get<double>())
        break;
      int nframes = 0;
      check(SAMPIC256CH_PrepareEvent(info.get(), params.get()), "PrepareEvent");
      const auto result =
          SAMPIC256CH_ReadEventBuffer(info.get(), 0, buffer, frames, &nframes);
      if (result == SAMPIC256CH_NoFrameRead) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        continue;
      }
      check(result, "ReadEventBuffer");
      if (nframes < 0 || nframes > MAX_EXPECTED_FRAMES)
        throw std::runtime_error("Invalid frame count");
      for (int i = 0; i < nframes; ++i) {
        if (frames[i].data_size < 0)
          throw std::runtime_error("Negative frame size");
        bytes += frames[i].data_size;
      }
      if (bytes >= config.at("max_frame_bytes").get<std::uint64_t>()) {
        status = "byte_limited";
        break;
      }
      if (nframes == 0)
        continue;
      int nhits = 0;
      check(SAMPIC256CH_DecodeEvent(info.get(), params.get(), frames,
                                    event.get(), nframes, &nhits),
            "DecodeEvent");
      if (nhits < 0 || nhits > MAX_EXPECTED_FRAMES)
        throw std::runtime_error("Invalid decoded hit count");
      ++events;
      for (int i = 0; i < nhits; ++i) {
        const auto &hit = event->Hit[i];
        if (hit.FeBoardIndex < 0 || hit.FeBoardIndex >= 4 || hit.Channel < 0 ||
            hit.Channel >= 64 || !selected[hit.FeBoardIndex * 64 + hit.Channel])
          throw std::runtime_error(
              "Unexpected disabled-channel hit; capture rejected");
        const auto key = std::to_string(hit.FeBoardIndex) + ":" +
                         std::to_string(hit.Channel);
        counts[key] = counts[key].get<std::uint64_t>() + 1;
        ++hits;
      }
      const double live =
          std::chrono::duration<double>(Clock::now() - begin).count();
      if (hits >= config.at("max_hits").get<std::uint64_t>()) {
        status = "hit_limited";
        break;
      }
      if (live >= config.at("pilot_s").get<double>() &&
          hits / live >= config.at("aggregate_rate_limit_hz").get<double>()) {
        status = "rate_limited";
        break;
      }
    }
    const double live =
        std::chrono::duration<double>(Clock::now() - begin).count();
    Json result = {
        {"schema_version", 1},
        {"status", status},
        {"elapsed_s", live},
        {"total_hits", hits},
        {"frame_bytes", bytes},
        {"events", events},
        {"counts", counts},
        {"applied_thresholds_v", applied},
        {"cleanup_verified", false},
        {"threshold_offset_specific_baseline", offset_specific_baseline}};
    result["vendor_library_version"] = SAMPIC_256CH_LIBRARY_VERSION;
    result["board_paths"] = Json::array();
    for (int i = 0; i < info->NbOfFeBoards; ++i)
      result["board_paths"].push_back(info->FrontEndBoardsPathIndex[i]);
    result["waveform_corrections"] = "disabled";
    result["frames_per_block"] = 1;
    return result;
  }
  void finish() {
    std::cout << "Stopping acquisition with vendor StopRun" << std::endl;
    if (started) {
      check(SAMPIC256CH_StopRun(info.get(), params.get()), "StopRun");
      started = false;
      std::cout << "Acquisition stopped; vendor StopRun completed" << std::endl;
    }
  }
};
int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr
        << "Usage: sampic_threshold_scan_worker request.json result.json\n";
    return 2;
  }
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  const std::filesystem::path result_path = argv[2];
  std::unique_ptr<Session> session;
  try {
    Json request;
    {
      std::ifstream in(argv[1]);
      in >> request;
    }
    commandRetryDelay = request.at("config").at("retry_delay_s").get<double>();
    const auto op = request.at("operation").get<std::string>();
    session = std::make_unique<Session>(request.at("config"));
    Json result;
    if (op == "discover") {
      if (session->info->NbOfFeBoards < 1 || session->info->NbOfFeBoards > 4)
        throw std::runtime_error("Invalid FEB discovery result");
      result = {{"schema_version", 1},
                {"board_count", session->info->NbOfFeBoards},
                {"paths", Json::array()}};
      for (int i = 0; i < session->info->NbOfFeBoards; ++i)
        result["paths"].push_back(session->info->FrontEndBoardsPathIndex[i]);
    } else if (op == "quiesce") {
      session->quiesce();
      result = {{"schema_version", 1}, {"cleanup_verified", true}};
    } else if (op == "measure") {
      result = session->measure(request);
      save(result_path.string() + ".pending", result);
      session->finish();
      result["cleanup_verified"] = true;
      result["cleanup_method"] = "vendor_stop_run";
    } else
      throw std::invalid_argument("Unknown operation");
    save(result_path, result);
    return result.value("status", "") == "interrupted" ? 130 : 0;
  } catch (const std::exception &error) {
    bool quiet = false;
    std::string cleanup;
    if (session) {
      try {
        if (session->started) {
          try {
            session->finish();
            quiet = true;
          } catch (const std::exception &stopError) {
            std::cerr
                << "StopRun failed; falling back to source-disable recovery: "
                << stopError.what() << std::endl;
          }
        }
        if (!quiet) {
          session->quiesce();
          quiet = true;
        }
      } catch (const std::exception &e) {
        cleanup = e.what();
      }
    }
    try {
      save(result_path, {{"schema_version", 1},
                         {"status", "error"},
                         {"error", error.what()},
                         {"cleanup_verified", quiet},
                         {"cleanup_error", cleanup}});
    } catch (...) {
    }
    std::cerr << error.what() << '\n';
    return 1;
  }
}
