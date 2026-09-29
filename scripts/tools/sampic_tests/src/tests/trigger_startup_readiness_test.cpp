// Focused demonstration of missing external-trigger records at run startup.
// All test-owned memory and the receiver thread exist before StartRun. The
// generator remains inhibited until that thread enters the vendor read loop.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "external_trigger_probe/sampic_session.h"
#include "sampic_tests/lecroy/lecroy_client.h"
#include "sampic_tests/lecroy/lecroy_output_gate.h"
#include "sampic_tests/modes/double_pulse/config.h"

namespace {
using Clock = std::chrono::steady_clock;
constexpr int kFramesPerBlock = 31;
constexpr int kTriggersPerEvent = 127;
constexpr double kTimestampScale = 3.0;

struct Arguments {
  std::filesystem::path config = "config/double_pulse_deadtime_scan.default.json";
  std::filesystem::path output = "data/trigger_startup_readiness_test/single_run";
  double rate_hz = 20000.0;
  double duration_s = 5.0;
};

Arguments ParseArguments(int argc, char** argv) {
  Arguments result;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    auto value = [&]() {
      if (++i >= argc) throw std::runtime_error("Missing value for " + argument);
      return std::string(argv[i]);
    };
    if (argument == "--rate-hz") result.rate_hz = std::stod(value());
    else if (argument == "--duration-s") result.duration_s = std::stod(value());
    else if (argument == "--output") result.output = value();
    else if (argument == "--config") result.config = value();
    else if (argument == "--help") {
      std::cout << "trigger_startup_readiness_test [--rate-hz HZ] "
                   "[--duration-s S] [--output DIR] [--config FILE]\n";
      std::exit(0);
    } else throw std::runtime_error("Unknown argument: " + argument);
  }
  if (result.rate_hz <= 0 || result.duration_s <= 0) {
    throw std::runtime_error("Rate and duration must be positive");
  }
  return result;
}

void ConfigurePulse(sampic::lecroy::LecroyClient& lecroy,
                    const std::string& channel) {
  sampic::lecroy::LecroyChannelConfig pulse;
  pulse.channel = channel;
  pulse.amplitude_v = 4.1;
  pulse.width_ns = 30.0;
  pulse.lead_ns = 1.4;
  pulse.trail_ns = 1.0;
  pulse.double_pulse_enabled = false;
  lecroy.SetChannelPulseParameters(channel, pulse, 0.0);
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const auto arguments = ParseArguments(argc, argv);
    const auto config =
        sampic::double_pulse::load_double_pulse_config(arguments.config);
    std::filesystem::create_directories(arguments.output);

    // Configure the pulse source while both outputs are inhibited.
    sampic::lecroy::LecroyClient lecroy;
    lecroy.Connect(config.lecroy.ip, config.lecroy.port);
    sampic::lecroy::LecroyOutputGate output_gate(lecroy, {"A", "B"});
    output_gate.Disable();
    ConfigurePulse(lecroy, "A");
    ConfigurePulse(lecroy, "B");
    lecroy.SetFrequency(arguments.rate_hz);
    const double rate_hz = std::stod(lecroy.Query("FREQ?"));

    // Maximum batching is intentional: smaller trigger batches caused severe
    // losses in the focused 10/20 kHz packetization test.
    SimpleSession session(config.connection, config.external_trigger, false);
    const int sampling_mhz = config.scan.digitizer_rates_mhz.empty()
        ? 6400 : config.scan.digitizer_rates_mhz.front();
    session.set_sampling_rate(sampling_mhz);
    session.set_packetization(kFramesPerBlock, kTriggersPerEvent);
    session.enable_channels(
        config.scan.board_index, {25, 26, 27, 28, 29, 30, 31});
    session.enable_plain_external_trigger();

    // Allocate all storage before StartRun. The capacity is deliberately much
    // larger than twice the expected records, including ten seconds of margin.
    auto event = std::make_unique<EventStruct>();
    std::vector<double> timestamps_ns;
    const auto expected = static_cast<std::size_t>(
        std::ceil(2.0 * rate_hz * (arguments.duration_s + 10.0)));
    timestamps_ns.reserve(std::max<std::size_t>(1000000, expected));
    const auto reserved_capacity = timestamps_ns.capacity();

    std::mutex mutex;
    std::condition_variable condition;
    bool may_read = false;
    bool abort = false;
    bool receiver_ready = false;
    Clock::time_point first_read_started;
    std::atomic<bool> cutoff{false};
    std::exception_ptr receiver_error;

    // Create the receiver (including its thread stack) before StartRun. It
    // waits until StartRun succeeds, then continuously calls the vendor API.
    const auto receiver_created = Clock::now();
    std::thread receiver([&] {
      try {
        {
          std::unique_lock lock(mutex);
          condition.wait(lock, [&] { return may_read || abort; });
          if (abort) return;
        }
        auto read_once = [&](const auto& readout) {
          {
            std::lock_guard lock(mutex);
            if (!receiver_ready) {
              first_read_started = Clock::now();
              receiver_ready = true;
            }
          }
          condition.notify_one();
          int hits = 0, frames = 0, bytes = 0;
          if (!session.read_event(
                  readout, *event, hits, frames, bytes, false)) return false;
          for (int i = 0; i < event->TriggerData.NbOfTriggers; ++i) {
            timestamps_ns.push_back(event->TriggerData.TriggerTimeStamp[i]);
          }
          return true;
        };
        while (!cutoff) read_once(config.readout);
        auto drain = config.readout;
        drain.retry_sleep_us = std::max(100, drain.retry_sleep_us);
        drain.max_loops =
            std::max(1, static_cast<int>(100000.0 / drain.retry_sleep_us));
        const auto deadline = Clock::now() + std::chrono::seconds(5);
        while (Clock::now() < deadline && read_once(drain)) {}
      } catch (...) { receiver_error = std::current_exception(); }
    });

    if (!session.start_run(config.start_retry)) {
      { std::lock_guard lock(mutex); abort = true; }
      condition.notify_one();
      receiver.join();
      throw std::runtime_error("SAMPIC StartRun failed");
    }
    { std::lock_guard lock(mutex); may_read = true; }
    condition.notify_one();
    {
      std::unique_lock lock(mutex);
      if (!condition.wait_for(
              lock, std::chrono::seconds(2), [&] { return receiver_ready; })) {
        lock.unlock();
        cutoff = true;
        receiver.join();
        session.stop_run();
        throw std::runtime_error("Receiver did not enter the read loop");
      }
    }

    Clock::time_point enable_started, enable_returned, disable_started;
    try {
      enable_started = Clock::now();
      output_gate.Enable();
      enable_returned = Clock::now();
      std::this_thread::sleep_until(
          enable_returned + std::chrono::duration<double>(arguments.duration_s));
      disable_started = Clock::now();
      output_gate.Disable();
      cutoff = true;
      receiver.join();
      session.stop_run();
    } catch (...) {
      try { output_gate.Disable(); } catch (...) {}
      cutoff = true;
      if (receiver.joinable()) receiver.join();
      session.stop_run();
      throw;
    }
    if (receiver_error) std::rethrow_exception(receiver_error);

    std::sort(timestamps_ns.begin(), timestamps_ns.end());
    const double period_ns = 1e9 / (kTimestampScale * rate_hz);
    int missing = 0;
    for (std::size_t i = 1; i < timestamps_ns.size(); ++i) {
      const double ratio = (timestamps_ns[i] - timestamps_ns[i - 1]) / period_ns;
      const int steps = std::max(1, static_cast<int>(std::llround(ratio)));
      if (steps > 1 && std::abs(ratio - steps) <= 0.30) missing += steps - 1;
    }
    const double wall_s =
        std::chrono::duration<double>(disable_started - enable_returned).count();
    const double span_s = timestamps_ns.size() > 1
        ? (timestamps_ns.back() - timestamps_ns.front()) / 1e9 : 0.0;
    const auto us_after_creation = [&](Clock::time_point time) {
      return std::chrono::duration<double, std::micro>(
                 time - receiver_created).count();
    };
    const nlohmann::json summary{
        {"rate_hz", rate_hz}, {"duration_s", wall_s},
        {"frames_per_block", kFramesPerBlock},
        {"triggers_per_event", kTriggersPerEvent},
        {"receiver_ready_before_enable", first_read_started < enable_started},
        {"first_read_started_us", us_after_creation(first_read_started)},
        {"lecroy_enable_started_us", us_after_creation(enable_started)},
        {"sampling_frequency_mhz", session.sampling_frequency_readback_mhz()},
        {"trigger_records", timestamps_ns.size()},
        {"inferred_internal_missing", missing},
        {"timestamp_span_s", span_s},
        {"timestamp_span_to_wall", wall_s > 0 ? span_s / wall_s : 0},
        {"timestamp_reallocation_occurred",
         timestamps_ns.capacity() != reserved_capacity}};

    // Files are opened only after disable, drain, and StopRun.
    std::ofstream summary_file(arguments.output / "summary.json");
    summary_file << std::setw(2) << summary << '\n';
    std::ofstream trigger_file(arguments.output / "triggers.csv");
    trigger_file << "index,timestamp_ns\n";
    for (std::size_t i = 0; i < timestamps_ns.size(); ++i) {
      trigger_file << i << ',' << std::setprecision(17) << timestamps_ns[i] << '\n';
    }
    std::cout << std::setw(2) << summary << "\nOutput: "
              << std::filesystem::absolute(arguments.output) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "trigger_startup_readiness_test: " << error.what() << '\n';
    return 1;
  }
}
