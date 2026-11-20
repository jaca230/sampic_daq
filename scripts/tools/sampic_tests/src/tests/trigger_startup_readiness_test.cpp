// Simple test for missing external-trigger records at run startup.
//
// The receiver thread is created before StartRun. After StartRun succeeds, the
// receiver enters the SAMPIC read loop. Only then do we enable the LeCroy. The
// recorded times prove that reading began before the first pulses were sent.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
}

namespace {
using Clock = std::chrono::steady_clock;

constexpr char kCrateIp[] = "192.168.0.14";
constexpr int kCratePort = 27014;
constexpr char kCalibrationDirectory[] =
    "resources/calib/Crate_PIONEER_N2_LPNHE";
constexpr char kLecroyIp[] = "10.0.1.103";
constexpr int kLecroyPort = 1234;
constexpr std::array<int, 7> kChannels{25, 26, 27, 28, 29, 30, 31};
constexpr int kFramesPerBlock = 31;
constexpr int kTriggersPerEvent = 127;

struct Arguments {
  double rate_hz = 1000.0;
  double duration_s = 5.0;
  std::filesystem::path output =
      "scripts/tools/sampic_tests/data/trigger_startup_readiness_test/single_run";
};

Arguments ParseArguments(int argc, char** argv) {
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string option = argv[i];
    if (option == "--help") {
      std::cout << "trigger_startup_readiness_test [--rate-hz HZ] "
                   "[--duration-s S] [--output DIR]\n";
      std::exit(0);
    }
    if (++i >= argc) throw std::runtime_error("Missing value for " + option);
    if (option == "--rate-hz") args.rate_hz = std::stod(argv[i]);
    else if (option == "--duration-s") args.duration_s = std::stod(argv[i]);
    else if (option == "--output") args.output = argv[i];
    else throw std::runtime_error("Unknown option: " + option);
  }
  return args;
}

void Check(SAMPIC256CH_ErrCode error, const char* call) {
  if (error != SAMPIC256CH_Success) {
    throw std::runtime_error(
        std::string(call) + " failed: " +
        std::to_string(static_cast<int>(error)));
  }
}

// This is the complete SAMPIC initialization sequence used by this test.
void InitializeSampic(CrateInfoStruct& info,
                      CrateParamStruct& params,
                      void*& event_buffer,
                      ML_Frame*& frames) {
  CrateConnectionParamStruct connection{};
  connection.ConnectionType = UDP_CONNECTION;
  connection.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(connection.CtrlIpAddress,
                sizeof(connection.CtrlIpAddress), "%s", kCrateIp);
  connection.CtrlPort = kCratePort;

  Check(SAMPIC256CH_OpenCrateConnection(connection, &info),
        "OpenCrateConnection");
  Check(SAMPIC256CH_CheckCrateFirmwareVersions(&info),
        "CheckCrateFirmwareVersions");
  Check(SAMPIC256CH_SetDefaultParameters(&info, &params),
        "SetDefaultParameters");

  const auto calibration = std::filesystem::absolute(kCalibrationDirectory);
  std::array<char, MAX_PATHNAME_LENGTH> directory{};
  std::snprintf(directory.data(), directory.size(), "%s",
                calibration.string().c_str());
  Check(SAMPIC256CH_LoadAllCalibValuesFromFiles(
            &info, &params, directory.data()),
        "LoadAllCalibValuesFromFiles");
  Check(SAMPIC256CH_AllocateEventMemory(&event_buffer, &frames),
        "AllocateEventMemory");

  // The library requires event memory before StopRun.
  SAMPIC256CH_StopRun(&info, &params);
}

// Configure only what this test needs: seven channels and external triggers.
void ConfigureSampic(CrateInfoStruct& info, CrateParamStruct& params) {
  Check(SAMPIC256CH_SetSamplingFrequency(&info, &params, 6400, FALSE),
        "SetSamplingFrequency");
  Check(SAMPIC256CH_SetChannelMode(
            &info, &params, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
        "DisableAllChannels");
  for (const int channel : kChannels) {
    Check(SAMPIC256CH_SetChannelMode(&info, &params, 0, channel, TRUE),
          "EnableChannel");
  }
  Check(SAMPIC256CH_SetSampicChannelTriggerMode(
            &info, &params, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
            SAMPIC_CHANNEL_EXT_TRIGGER_MODE),
        "SetExternalTriggerMode");
  Check(SAMPIC256CH_SetExternalTriggerType(&info, &params, EXT_SIG),
        "SetExternalTriggerType");
  Check(SAMPIC256CH_SetExternalTriggerEdge(&info, &params, RISING_EDGE),
        "SetExternalTriggerEdge");
  Check(SAMPIC256CH_SetExternalTriggerSigLevel(&info, &params, TTL_SIG),
        "SetExternalTriggerLevel");
  Check(SAMPIC256CH_SetExternalTriggerCounterMode(
            &info, &params, TRUE, TRUE),
        "SetExternalTriggerCounterMode");
  Check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(
            &info, &params, kTriggersPerEvent),
        "SetMinNbOfTriggersPerEvent");
}

// Read one decoded vendor event. False means no data arrived before timeout.
bool ReadEvent(CrateInfoStruct& info,
               CrateParamStruct& params,
               void* event_buffer,
               ML_Frame* frames,
               EventStruct& event,
               std::vector<double>& timestamps,
               std::chrono::milliseconds timeout) {
  SAMPIC256CH_PrepareEvent(&info, &params);
  const auto deadline = Clock::now() + timeout;
  int frame_count = 0;
  auto error = SAMPIC256CH_NoFrameRead;

  while (error != SAMPIC256CH_Success && Clock::now() < deadline) {
    error = SAMPIC256CH_ReadEventBuffer(
        &info, 0, event_buffer, frames, &frame_count);
    if (error != SAMPIC256CH_Success) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  if (error != SAMPIC256CH_Success) return false;

  int hit_count = 0;
  error = SAMPIC256CH_DecodeEvent(
      &info, &params, frames, &event, frame_count, &hit_count);
  if (error != SAMPIC256CH_Success) return false;
  for (int i = 0; i < event.TriggerData.NbOfTriggers; ++i) {
    timestamps.push_back(event.TriggerData.TriggerTimeStamp[i]);
  }
  return true;
}

std::vector<double> FindMissing(std::vector<double>& timestamps,
                                double rate_hz) {
  std::sort(timestamps.begin(), timestamps.end());
  std::vector<double> missing_locations_ns;
  if (timestamps.empty()) return missing_locations_ns;

  const double expected_period_ns = 1e9 / rate_hz;
  for (std::size_t i = 1; i < timestamps.size(); ++i) {
    const double periods =
        (timestamps[i] - timestamps[i - 1]) / expected_period_ns;
    const int rounded = std::max(1, static_cast<int>(std::llround(periods)));
    if (rounded > 1 && std::abs(periods - rounded) < 0.30) {
      for (int step = 1; step < rounded; ++step) {
        const double inferred_timestamp =
            timestamps[i - 1] + step * expected_period_ns;
        missing_locations_ns.push_back(
            inferred_timestamp - timestamps.front());
      }
    }
  }
  return missing_locations_ns;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const auto args = ParseArguments(argc, argv);

    // Configure the LeCroy, but keep both outputs disabled.
    sampic::lecroy::LecroyClient lecroy;
    lecroy.Connect(kLecroyIp, kLecroyPort);
    sampic::lecroy::LecroyOutputGate output_gate(lecroy, {"A", "B"});
    output_gate.Disable();
    lecroy.SetFrequency(args.rate_hz);
    const double rate_hz = std::stod(lecroy.Query("FREQ?"));

    // Initialize and configure SAMPIC.
    CrateInfoStruct info{};
    CrateParamStruct params{};
    void* event_buffer = nullptr;
    ML_Frame* frames = nullptr;
    InitializeSampic(info, params, event_buffer, frames);
    ConfigureSampic(info, params);

    auto event = std::make_unique<EventStruct>();
    std::vector<double> timestamps;
    timestamps.reserve(static_cast<std::size_t>(
        2 * rate_hz * (args.duration_s + 2)));

    // Create the receiver before StartRun. It waits until StartRun succeeds.
    std::atomic<bool> start_reading{false};
    std::atomic<bool> receiver_ready{false};
    std::atomic<bool> stop_reading{false};
    Clock::time_point first_read_started;
    const auto receiver_created = Clock::now();
    std::thread receiver([&] {
      while (!start_reading) std::this_thread::yield();
      first_read_started = Clock::now();
      receiver_ready = true;
      while (!stop_reading) {
        ReadEvent(info, params, event_buffer, frames, *event, timestamps,
                  std::chrono::milliseconds(1000));
      }
      // Drain packets already buffered after the pulse source is disabled.
      while (ReadEvent(info, params, event_buffer, frames, *event, timestamps,
                       std::chrono::milliseconds(100))) {}
    });

    // StopRun changes frames/block to one, so restore it just before StartRun.
    Check(SAMPIC256CH_SetNbOfFramesPerBlock(
              &info, &params, kFramesPerBlock),
          "SetNbOfFramesPerBlock");
    const auto start_error = SAMPIC256CH_StartRun(&info, &params, TRUE);
    if (start_error != SAMPIC256CH_Success) {
      stop_reading = true;
      start_reading = true;
      receiver.join();
      Check(start_error, "StartRun");
    }

    // Enter the read loop first, then enable pulses for the requested time.
    start_reading = true;
    while (!receiver_ready) std::this_thread::yield();
    const auto lecroy_enable_started = Clock::now();
    output_gate.Enable();
    const auto lecroy_enable_returned = Clock::now();
    std::this_thread::sleep_for(std::chrono::duration<double>(args.duration_s));
    output_gate.Disable();

    stop_reading = true;
    receiver.join();
    Check(SAMPIC256CH_StopRun(&info, &params), "StopRun");

    // Acquisition is finished before any output file is opened.
    SAMPIC256CH_FreeEventMemory(&event_buffer, &frames);
    SAMPIC256CH_CloseCrateConnection(&info);

    const auto missing_locations_ns = FindMissing(timestamps, rate_hz);
    const auto elapsed_us = [&](Clock::time_point time) {
      return std::chrono::duration<double, std::micro>(
                 time - receiver_created).count();
    };
    const double span_s = timestamps.size() > 1
        ? (timestamps.back() - timestamps.front()) / 1e9
        : 0.0;
    const nlohmann::json summary{
        {"rate_hz", rate_hz},
        {"duration_s", args.duration_s},
        {"first_read_started_us", elapsed_us(first_read_started)},
        {"lecroy_enable_started_us", elapsed_us(lecroy_enable_started)},
        {"lecroy_enable_returned_us", elapsed_us(lecroy_enable_returned)},
        {"receiver_ready_before_enable",
         first_read_started < lecroy_enable_started},
        {"trigger_records", timestamps.size()},
        {"inferred_internal_missing", missing_locations_ns.size()},
        {"inferred_internal_missing_locations_ns", missing_locations_ns},
        {"timestamp_span_s", span_s}};

    std::filesystem::create_directories(args.output);
    std::ofstream summary_file(args.output / "summary.json");
    summary_file << std::setw(2) << summary << '\n';
    std::ofstream trigger_file(args.output / "triggers.csv");
    trigger_file << "index,timestamp_ns\n";
    for (std::size_t i = 0; i < timestamps.size(); ++i) {
      trigger_file << i << ',' << std::setprecision(17)
                   << timestamps[i] << '\n';
    }
    std::cout << std::setw(2) << summary << "\nOutput: "
              << std::filesystem::absolute(args.output) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "trigger_startup_readiness_test: " << error.what() << '\n';
    return 1;
  }
}
