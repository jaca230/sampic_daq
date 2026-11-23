#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>
#include "external_trigger_probe/sampic_session.h"
#include "sampic_tests/modes/double_pulse/config.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
struct Selection { int board; std::vector<int> channels; };
struct Config {
  fs::path hardware, output_root;
  std::vector<int> frequencies;
  std::vector<Selection> selections;
  double duration_s;
  double threshold_v;
  std::size_t max_waveforms_per_channel;
  bool reconnect_each_frequency;
  int frames_per_block, triggers_per_event;
};

Config load_config(const fs::path& path) {
  std::ifstream input(path); json j; input >> j;
  if (!input) throw std::runtime_error("Cannot read " + path.string());
  Config c;
  c.hardware = j.at("hardware_config").get<std::string>();
  if (c.hardware.is_relative()) c.hardware = path.parent_path() / c.hardware;
  c.hardware = fs::weakly_canonical(c.hardware);
  c.output_root = j.at("output_root").get<std::string>();
  c.frequencies = j.at("sampling_frequencies_mhz").get<std::vector<int>>();
  c.duration_s = j.value("acquisition_s", 2.0);
  c.threshold_v = j.value("threshold_v", -1.0);
  c.max_waveforms_per_channel =
      j.value("max_waveforms_per_channel", std::size_t{100});
  c.reconnect_each_frequency = j.value("reconnect_each_frequency", false);
  c.frames_per_block = j.value("frames_per_block", 31);
  c.triggers_per_event = j.value("triggers_per_event", 127);
  for (const auto& group : j.at("channel_groups"))
    c.selections.push_back({group.at("board").get<int>(),
                            group.at("channels").get<std::vector<int>>()});
  if (c.frequencies.empty() || c.selections.empty() || c.duration_s <= 0)
    throw std::runtime_error("Empty or invalid scan configuration");
  return c;
}

std::string timestamp_name() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm tm{}; localtime_r(&value, &tm);
  std::ostringstream out; out << std::put_time(&tm, "%Y%m%d_%H%M%S");
  return out.str();
}

double median(std::vector<double> values) {
  if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
  std::sort(values.begin(), values.end());
  const auto mid = values.size() / 2;
  return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2;
}

double half_height_width_samples(const HitStruct& hit) {
  const int count = std::clamp(hit.DataSize, 0, MAX_NB_OF_SAMPLES);
  if (count < 3) return NAN;
  const float* samples = hit.CorrectedDataSamples;
  const auto peak_it = std::max_element(samples, samples + count);
  const int peak = static_cast<int>(peak_it - samples);
  const double level = hit.Baseline + 0.5 * (*peak_it - hit.Baseline);
  if (!(*peak_it > hit.Baseline)) return NAN;
  int left = peak, right = peak;
  while (left > 0 && samples[left] >= level) --left;
  while (right + 1 < count && samples[right] >= level) ++right;
  if (left == 0 || right + 1 == count) return NAN;
  const auto crossing = [&](int a, int b) {
    const double delta = samples[b] - samples[a];
    return std::abs(delta) < 1e-12 ? static_cast<double>(b)
                                   : a + (level - samples[a]) / delta;
  };
  return crossing(right + 1, right) - crossing(left, left + 1);
}

struct Options { fs::path config, output; bool dry_run = false; };
Options options(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) o.config = argv[++i];
    else if (arg == "--output-dir" && i + 1 < argc) o.output = argv[++i];
    else if (arg == "--dry-run") o.dry_run = true;
    else if (arg == "--help" || arg == "-h") {
      std::cout << "sampling_frequency_waveform_scan --config FILE "
                   "[--output-dir DIR] [--dry-run]\n";
      std::exit(0);
    } else throw std::runtime_error("Unknown or incomplete argument: " + arg);
  }
  if (o.config.empty()) throw std::runtime_error("--config is required");
  return o;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = options(argc, argv);
    const auto scan = load_config(opts.config);
    auto hardware = sampic::double_pulse::load_double_pulse_config(scan.hardware);
    hardware.connection.use_external_trigger = false;
    if (scan.threshold_v > 0) hardware.connection.threshold_volts = scan.threshold_v;
    const fs::path output = opts.output.empty()
        ? scan.output_root / timestamp_name() : opts.output;

    std::cout << "Single-crate sampling-frequency timestamp scan\n  crate: "
              << hardware.connection.ip << ':' << hardware.connection.port
              << "\n  frequencies [MHz]: ";
    for (std::size_t i = 0; i < scan.frequencies.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << scan.frequencies[i];
    }
    std::cout << "\n  pulser: left untouched\n  output: " << output << '\n';
    std::cout << "  connection mode: "
              << (scan.reconnect_each_frequency ? "reconnect each frequency"
                                                : "one persistent connection")
              << '\n';
    if (opts.dry_run) return 0;

    fs::create_directories(output);
    fs::copy_file(opts.config, output / "scan_config.json",
                  fs::copy_options::overwrite_existing);
    fs::copy_file(scan.hardware, output / "hardware_config.json",
                  fs::copy_options::overwrite_existing);
    std::ofstream summary(output / "summary.csv");
    summary << "point,requested_mhz,readback_mhz,events,hits,first_timestamp_ns,"
               "last_timestamp_ns,timestamp_span_s,intervals,"
               "median_interval_ns,mean_interval_ns,stddev_interval_ns,"
               "inferred_rate_hz,captured_waveforms,"
               "median_half_height_width_samples,width_ns_using_requested_frequency\n";
    std::ofstream detail(output / "timestamps.csv");
    detail << "point,requested_mhz,readback_mhz,feb,sampic,channel,index,"
              "first_cell_timestamp_ns\n";
    std::ofstream waveforms(output / "waveforms.csv");
    waveforms << "point,requested_mhz,readback_mhz,event_index,hit_index,feb,"
                 "sampic,channel,first_cell_timestamp_ns,trigger_position_cell,"
                 "half_height_width_samples,sample_index,corrected_v,raw_adc\n";

    auto event = std::make_unique<EventStruct>();
    std::unique_ptr<SimpleSession> persistent_session;
    if (!scan.reconnect_each_frequency) {
      persistent_session = std::make_unique<SimpleSession>(
          hardware.connection, hardware.external_trigger, true,
          /*defer_trigger_configuration=*/true);
    }

    int point = 0;
    for (const int requested : scan.frequencies) {
      ++point;
      // Reproduce Pascal's systemInit -> systemSet -> systemTrigger hardware
      // order. By default the socket and host event buffers persist while the
      // crate itself is reset/reinitialized between points; the optional mode
      // also recreates the transport session for comparison.
      std::unique_ptr<SimpleSession> point_session;
      if (scan.reconnect_each_frequency) {
        point_session = std::make_unique<SimpleSession>(
            hardware.connection, hardware.external_trigger, true,
            /*defer_trigger_configuration=*/true);
      } else if (point > 1) {
        persistent_session->reinitialize_hardware_like_pascal();
      }
      SimpleSession& session = scan.reconnect_each_frequency
          ? *point_session : *persistent_session;
      session.set_packetization(scan.frames_per_block, scan.triggers_per_event);
      session.set_sampling_rate(requested);
      session.configure_trigger_defaults();
      for (const auto& selection : scan.selections)
        session.enable_channels(selection.board, selection.channels);
      session.enable_plain_self_trigger();
      const int readback = session.sampling_frequency_readback_mhz();
      if (!session.start_run(hardware.start_retry))
        throw std::runtime_error("StartRun failed at " + std::to_string(requested));

      std::map<std::tuple<int, int, int>, std::vector<double>> channels;
      std::map<std::tuple<int, int, int>, std::size_t> captured_by_channel;
      std::vector<double> waveform_widths;
      std::size_t events = 0, hits_total = 0;
      const auto start = std::chrono::steady_clock::now();
      const auto deadline = start + std::chrono::duration<double>(scan.duration_s);
      try {
        while (std::chrono::steady_clock::now() < deadline) {
          int hits = 0, frames = 0, bytes = 0;
          if (!session.read_event(hardware.readout, *event, hits, frames, bytes, false))
            continue;
          const std::size_t event_index = events++;
          hits_total += std::max(0, hits);
          for (int i = 0; i < hits; ++i) {
            const auto& hit = event->Hit[i];
            const auto key =
                std::make_tuple(hit.FeBoardIndex, hit.SampicIndex, hit.Channel);
            channels[key].push_back(hit.FirstCellTimeStamp);
            if (captured_by_channel[key] >= scan.max_waveforms_per_channel) continue;
            ++captured_by_channel[key];
            const double width = half_height_width_samples(hit);
            if (std::isfinite(width)) waveform_widths.push_back(width);
            const int samples = std::clamp(hit.DataSize, 0, MAX_NB_OF_SAMPLES);
            for (int sample = 0; sample < samples; ++sample) {
              waveforms << point << ',' << requested << ',' << readback << ','
                        << event_index << ',' << i << ',' << hit.FeBoardIndex << ','
                        << hit.SampicIndex << ',' << hit.Channel << ','
                        << std::setprecision(17) << hit.FirstCellTimeStamp << ','
                        << hit.AdvancedParams.FirstTriggerPositionCell << ',' << width
                        << ',' << sample << ',' << hit.CorrectedDataSamples[sample]
                        << ',' << hit.OrderedRawDataSamples[sample] << '\n';
            }
          }
        }
        session.stop_run();
      } catch (...) { session.stop_run(); throw; }
      std::vector<double> all, intervals;
      for (auto& [key, times] : channels) {
        std::sort(times.begin(), times.end());
        all.insert(all.end(), times.begin(), times.end());
        const auto [feb, sampic, channel] = key;
        for (std::size_t i = 0; i < times.size(); ++i) {
          detail << point << ',' << requested << ',' << readback << ',' << feb << ',' << sampic
                 << ',' << channel << ',' << i << ',' << std::setprecision(17)
                 << times[i] << '\n';
          if (i && times[i] > times[i - 1]) intervals.push_back(times[i] - times[i - 1]);
        }
      }
      std::sort(all.begin(), all.end());
      const double first = all.empty() ? NAN : all.front();
      const double last = all.empty() ? NAN : all.back();
      const double span_s = all.size() > 1 ? (last - first) / 1e9 : 0;
      const double med = median(intervals);
      const double mean = intervals.empty() ? NAN
          : std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();
      double variance = 0;
      for (double x : intervals) variance += (x - mean) * (x - mean);
      const double sd = intervals.empty() ? NAN : std::sqrt(variance / intervals.size());
      const double rate = med > 0 ? 1e9 / med : NAN;
      const std::size_t captured = std::accumulate(
          captured_by_channel.begin(), captured_by_channel.end(), std::size_t{0},
          [](std::size_t total, const auto& item) { return total + item.second; });
      const double width_samples = median(waveform_widths);
      const double width_ns = width_samples * 1000.0 / requested;
      summary << std::setprecision(17) << point << ',' << requested << ',' << readback
              << ',' << events << ',' << hits_total << ',' << first << ',' << last
              << ',' << span_s << ',' << intervals.size() << ',' << med << ',' << mean
              << ',' << sd << ',' << rate << ',' << captured << ',' << width_samples
              << ',' << width_ns << '\n';
      summary.flush(); detail.flush(); waveforms.flush();
      std::cout << "  point " << point << ", " << requested
                << " MHz: readback=" << readback << ", timestamp span=" << span_s
                << " s, median dt=" << med << " ns (" << rate
                << " Hz), median width=" << width_samples << " samples ("
                << width_ns << " ns using requested frequency), hits="
                << hits_total << '\n';
    }
    std::cout << "Scan complete: " << fs::absolute(output) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "sampling_frequency_waveform_scan: " << error.what() << '\n';
    return 1;
  }
}
