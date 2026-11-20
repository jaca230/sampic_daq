// Compare proper and improper ordering of sampling-frequency changes.
// A fixed 1 kHz LeCroy pulse should always produce a 1 ms median timestamp gap.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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
constexpr double kPulserRateHz = 1000.0;
constexpr int kBoard = 0;
constexpr int kChannel = 31;
constexpr int kFramesPerBlock = 31;
constexpr std::array<int, 7> kFrequenciesMhz{
    2133, 6400, 1600, 4252, 3200, 2133, 6400};

void Check(SAMPIC256CH_ErrCode error, const char* call) {
  if (error != SAMPIC256CH_Success) {
    throw std::runtime_error(
        std::string(call) + " failed: " +
        std::to_string(static_cast<int>(error)));
  }
}

void LoadCalibration(CrateInfoStruct& info, CrateParamStruct& params) {
  const auto calibration = std::filesystem::absolute(kCalibrationDirectory);
  std::array<char, MAX_PATHNAME_LENGTH> directory{};
  std::snprintf(directory.data(), directory.size(), "%s",
                calibration.string().c_str());
  Check(SAMPIC256CH_LoadAllCalibValuesFromFiles(
            &info, &params, directory.data()),
        "LoadAllCalibValuesFromFiles");
}

void Initialize(CrateInfoStruct& info,
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
  LoadCalibration(info, params);
  Check(SAMPIC256CH_AllocateEventMemory(&event_buffer, &frames),
        "AllocateEventMemory");
  SAMPIC256CH_StopRun(&info, &params);
}

void ConfigureSelfTrigger(CrateInfoStruct& info, CrateParamStruct& params) {
  Check(SAMPIC256CH_SetChannelMode(
            &info, &params, ALL_FE_BOARDs, ALL_CHANNELs, FALSE),
        "DisableAllChannels");
  Check(SAMPIC256CH_SetChannelMode(
            &info, &params, kBoard, kChannel, TRUE),
        "EnableChannel31");
  Check(SAMPIC256CH_SetSampicChannelTriggerMode(
            &info, &params, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
            SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
        "SetSelfTriggerMode");
  Check(SAMPIC256CH_SetChannelSelflTriggerEdge(
            &info, &params, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs,
            RISING_EDGE),
        "SetSelfTriggerEdge");
  Check(SAMPIC256CH_SetSampicChannelPulseMode(
            &info, &params, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs, TRUE),
        "SetPositivePulseMode");
  Check(SAMPIC256CH_SetSampicChannelInternalThreshold(
            &info, &params, ALL_FE_BOARDs, ALL_SAMPICs, ALL_CHANNELs, 0.1f),
        "SetThreshold");
}

bool ReadEvent(CrateInfoStruct& info,
               CrateParamStruct& params,
               void* event_buffer,
               ML_Frame* frames,
               EventStruct& event,
               int& hit_count) {
  SAMPIC256CH_PrepareEvent(&info, &params);
  int frame_count = 0;
  auto error = SAMPIC256CH_NoFrameRead;
  const auto deadline = Clock::now() + std::chrono::milliseconds(100);
  while (error != SAMPIC256CH_Success && Clock::now() < deadline) {
    error = SAMPIC256CH_ReadEventBuffer(
        &info, 0, event_buffer, frames, &frame_count);
    if (error != SAMPIC256CH_Success) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  if (error != SAMPIC256CH_Success) return false;
  return SAMPIC256CH_DecodeEvent(
             &info, &params, frames, &event, frame_count, &hit_count) ==
         SAMPIC256CH_Success;
}

double Median(std::vector<double> values) {
  if (values.empty()) return NAN;
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  return values.size() % 2
      ? values[middle]
      : (values[middle - 1] + values[middle]) / 2.0;
}

double MeasureMedianInterval(CrateInfoStruct& info,
                             CrateParamStruct& params,
                             void* event_buffer,
                             ML_Frame* frames) {
  auto event = std::make_unique<EventStruct>();
  std::vector<double> timestamps;
  const auto deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < deadline) {
    int hit_count = 0;
    if (!ReadEvent(info, params, event_buffer, frames, *event, hit_count)) {
      continue;
    }
    for (int i = 0; i < hit_count; ++i) {
      const auto& hit = event->Hit[i];
      if (hit.FeBoardIndex == kBoard && hit.Channel == kChannel) {
        timestamps.push_back(hit.FirstCellTimeStamp);
      }
    }
  }

  std::sort(timestamps.begin(), timestamps.end());
  std::vector<double> intervals;
  for (std::size_t i = 1; i < timestamps.size(); ++i) {
    if (timestamps[i] > timestamps[i - 1]) {
      intervals.push_back(timestamps[i] - timestamps[i - 1]);
    }
  }
  return Median(std::move(intervals));
}
}  // namespace

int main(int argc, char** argv) {
  try {
    std::filesystem::path output =
        "scripts/tools/sampic_tests/data/sampling_frequency_order_test.csv";
    if (argc == 3 && std::string(argv[1]) == "--output") output = argv[2];
    else if (argc != 1) {
      std::cout << "sampling_frequency_order_test [--output FILE]\n";
      return 0;
    }

    // Hold the LeCroy at one known rate for the entire comparison.
    sampic::lecroy::LecroyClient lecroy;
    lecroy.Connect(kLecroyIp, kLecroyPort);
    lecroy.SetFrequency(kPulserRateHz);
    const double actual_rate_hz = std::stod(lecroy.Query("FREQ?"));
    sampic::lecroy::LecroyOutputGate output_gate(lecroy, {"A", "B"});
    output_gate.Enable();
    const double expected_interval_ns = 1e9 / actual_rate_hz;

    std::filesystem::create_directories(output.parent_path());
    std::ofstream results(output);
    results << "mode,point,requested_mhz,readback_mhz,expected_interval_ns,"
               "median_interval_ns,measured_over_expected\n";

    for (const bool proper_order : {true, false}) {
      const std::string mode = proper_order ? "proper_order" : "improper_order";

      // Start each comparison from a fresh connection and identical defaults.
      CrateInfoStruct info{};
      CrateParamStruct params{};
      void* event_buffer = nullptr;
      ML_Frame* frames = nullptr;
      Initialize(info, params, event_buffer, frames);
      ConfigureSelfTrigger(info, params);

      int point = 0;
      for (const int requested_mhz : kFrequenciesMhz) {
        ++point;
        SAMPIC256CH_StopRun(&info, &params);

        // The old ordering configured trigger/readout settings before changing
        // the sampling frequency. The proper ordering changes the frequency
        // first and restores frames/block immediately before StartRun.
        if (!proper_order) {
          ConfigureSelfTrigger(info, params);
          Check(SAMPIC256CH_SetNbOfFramesPerBlock(
                    &info, &params, kFramesPerBlock),
                "SetNbOfFramesPerBlock");
        }
        Check(SAMPIC256CH_SetSamplingFrequency(
                  &info, &params, requested_mhz, FALSE),
              "SetSamplingFrequency");

        int readback_mhz = 0;
        Boolean external_clock = FALSE;
        Check(SAMPIC256CH_GetSamplingFrequency(
                  &params, &readback_mhz, &external_clock),
              "GetSamplingFrequency");

        if (proper_order) {
          Check(SAMPIC256CH_SetNbOfFramesPerBlock(
                    &info, &params, kFramesPerBlock),
                "SetNbOfFramesPerBlock");
        }
        Check(SAMPIC256CH_StartRun(&info, &params, TRUE), "StartRun");
        const double median_ns =
            MeasureMedianInterval(info, params, event_buffer, frames);
        Check(SAMPIC256CH_StopRun(&info, &params), "StopRun");

        const double ratio = median_ns / expected_interval_ns;
        results << std::setprecision(17)
                << mode << ',' << point << ',' << requested_mhz << ','
                << readback_mhz << ',' << expected_interval_ns << ','
                << median_ns << ',' << ratio << '\n';
        results.flush();
        std::cout << mode << "  requested=" << requested_mhz
                  << " MHz  readback=" << readback_mhz
                  << " MHz  median_dt=" << median_ns
                  << " ns  expected=" << expected_interval_ns
                  << " ns  ratio=" << ratio << '\n';
      }

      SAMPIC256CH_FreeEventMemory(&event_buffer, &frames);
      SAMPIC256CH_CloseCrateConnection(&info);
    }

    output_gate.Disable();
    std::cout << "Results: " << std::filesystem::absolute(output) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "sampling_frequency_order_test: " << error.what() << '\n';
    return 1;
  }
}
