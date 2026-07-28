#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

struct Options {
  std::string config_path;
  bool apply = false;
  bool without_gate = false;
  bool compare = false;
  bool b_output_gate_test = false;
};

struct Config {
  std::string ip = "192.168.0.4";
  int port = 27015;
  int board = 0;
  std::vector<int> channels{0};
  int sampling_mhz = 6400;
  float threshold_volts = 0.1F;
  bool load_calibration = false;
  std::string calibration_dir = "resources/calib";
  unsigned char primitive_gate_clocks = 10;
  unsigned char latency_gate_clocks = 3;
  unsigned char external_gate_clocks = 5;
  bool enable_trigger_counter = true;
  bool detect_external_trigger_id = true;
  unsigned char trigger_records_per_frame = 1;
  double expected_hit_minus_trigger_ns = 0.0;
  double correlation_tolerance_ns = 100.0;
  int max_events = 100;
  double duration_s = 10.0;
  double comparison_duration_s = 5.0;
  int readout_max_loops = 10000;
  int readout_retry_sleep_us = 100;
  EdgeType_t external_edge = RISING_EDGE;
  SignalLevel_t external_level = TTL_SIG;
  std::string lecroy_ip = "10.0.1.103";
  int lecroy_port = 1234;
  std::string lecroy_gate_channel = "B";
};

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == "--config") {
      if (++i == argc) throw std::runtime_error("--config requires a path");
      opts.config_path = argv[i];
    } else if (arg == "--apply") {
      opts.apply = true;
    } else if (arg == "--without-gate") {
      opts.without_gate = true;
    } else if (arg == "--compare") {
      opts.compare = true;
    } else if (arg == "--b-output-gate-test") {
      opts.b_output_gate_test = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: l2_external_gate_probe --config <file> --apply "
                   "[--without-gate|--compare|--b-output-gate-test]\n"
                   "\n"
                   "Without --apply the program only validates and prints the requested setup.\n"
                   "--without-gate is the ungated baseline: it leaves the L2 build enabled but\n"
                   "disables coincidence with the external gate.\n"
                   "--compare takes sequential ungated and gated measurements and reports\n"
                   "per-channel event/hit rates.\n"
                   "--b-output-gate-test temporarily disables only the configured Lecroy B\n"
                   "output to prove that the SAMPIC L2 gate rejects events without it.\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown argument: " + std::string(arg));
    }
  }
  if (opts.config_path.empty()) throw std::runtime_error("--config is required");
  const int test_modes = int(opts.without_gate) + int(opts.compare) + int(opts.b_output_gate_test);
  if (test_modes > 1) {
    throw std::runtime_error("--without-gate, --compare, and --b-output-gate-test are exclusive");
  }
  return opts;
}

std::string upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return value;
}

EdgeType_t parse_edge(const std::string& value) {
  const auto text = upper(value);
  if (text == "RISING" || text == "RISING_EDGE") return RISING_EDGE;
  if (text == "FALLING" || text == "FALLING_EDGE") return FALLING_EDGE;
  throw std::runtime_error("external_trigger.edge must be RISING or FALLING");
}

SignalLevel_t parse_level(const std::string& value) {
  const auto text = upper(value);
  if (text == "TTL") return TTL_SIG;
  if (text == "NIM") return NIM_SIG;
  throw std::runtime_error("external_trigger.signal_level must be TTL or NIM");
}

Config load_config(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("Cannot open configuration: " + path);
  const nlohmann::json doc = nlohmann::json::parse(input);
  Config cfg;

  if (const auto& node = doc.value("connection", nlohmann::json::object()); !node.empty()) {
    cfg.ip = node.value("ip", cfg.ip);
    cfg.port = node.value("port", cfg.port);
    cfg.load_calibration = node.value("load_calibration", cfg.load_calibration);
    cfg.calibration_dir = node.value("calibration_dir", cfg.calibration_dir);
  }
  if (const auto& node = doc.value("acquisition", nlohmann::json::object()); !node.empty()) {
    cfg.board = node.value("board", cfg.board);
    cfg.channels = node.value("channels", cfg.channels);
    cfg.sampling_mhz = node.value("sampling_mhz", cfg.sampling_mhz);
    cfg.threshold_volts = node.value("threshold_volts", cfg.threshold_volts);
  }
  if (const auto& node = doc.value("external_trigger", nlohmann::json::object()); !node.empty()) {
    cfg.external_edge = parse_edge(node.value("edge", std::string{"RISING"}));
    cfg.external_level = parse_level(node.value("signal_level", std::string{"TTL"}));
    cfg.enable_trigger_counter = node.value("enable_counter", cfg.enable_trigger_counter);
    cfg.detect_external_trigger_id =
        node.value("detect_external_trigger_id", cfg.detect_external_trigger_id);
    cfg.trigger_records_per_frame =
        node.value("records_per_frame", cfg.trigger_records_per_frame);
  }
  if (const auto& node = doc.value("l2_gate", nlohmann::json::object()); !node.empty()) {
    cfg.primitive_gate_clocks = node.value("primitive_gate_clocks", cfg.primitive_gate_clocks);
    cfg.latency_gate_clocks = node.value("latency_gate_clocks", cfg.latency_gate_clocks);
    cfg.external_gate_clocks = node.value("external_gate_clocks", cfg.external_gate_clocks);
  }
  if (const auto& node = doc.value("readout", nlohmann::json::object()); !node.empty()) {
    cfg.max_events = node.value("max_events", cfg.max_events);
    cfg.duration_s = node.value("duration_s", cfg.duration_s);
    cfg.readout_max_loops = node.value("max_loops", cfg.readout_max_loops);
    cfg.readout_retry_sleep_us = node.value("retry_sleep_us", cfg.readout_retry_sleep_us);
  }
  if (const auto& node = doc.value("comparison", nlohmann::json::object()); !node.empty()) {
    cfg.comparison_duration_s = node.value("duration_s", cfg.comparison_duration_s);
  }
  if (const auto& node = doc.value("correlation", nlohmann::json::object()); !node.empty()) {
    cfg.expected_hit_minus_trigger_ns =
        node.value("expected_hit_minus_trigger_ns", cfg.expected_hit_minus_trigger_ns);
    cfg.correlation_tolerance_ns =
        node.value("tolerance_ns", cfg.correlation_tolerance_ns);
  }
  if (const auto& node = doc.value("lecroy", nlohmann::json::object()); !node.empty()) {
    cfg.lecroy_ip = node.value("ip", cfg.lecroy_ip);
    cfg.lecroy_port = node.value("port", cfg.lecroy_port);
    cfg.lecroy_gate_channel = upper(node.value("gate_channel", cfg.lecroy_gate_channel));
  }

  if (cfg.board < 0 || cfg.board >= MAX_NB_OF_FE_BOARDS) {
    throw std::runtime_error("acquisition.board must be between 0 and 3");
  }
  if (cfg.channels.empty()) throw std::runtime_error("acquisition.channels must not be empty");
  for (const int channel : cfg.channels) {
    if (channel < 0 || channel >= 64) {
      throw std::runtime_error("channel out of range (0-63): " + std::to_string(channel));
    }
  }
  if (cfg.external_gate_clocks < 3) {
    throw std::runtime_error("l2_gate.external_gate_clocks must be at least 3 (30 ns)");
  }
  if (cfg.trigger_records_per_frame == 0 || cfg.trigger_records_per_frame > 127) {
    throw std::runtime_error("external_trigger.records_per_frame must be between 1 and 127");
  }
  if (cfg.correlation_tolerance_ns <= 0.0) {
    throw std::runtime_error("correlation.tolerance_ns must be positive");
  }
  if (cfg.comparison_duration_s <= 0.0) {
    throw std::runtime_error("comparison.duration_s must be positive");
  }
  if (cfg.lecroy_gate_channel != "A" && cfg.lecroy_gate_channel != "B") {
    throw std::runtime_error("lecroy.gate_channel must be A or B");
  }
  return cfg;
}

void check(SAMPIC256CH_ErrCode code, std::string_view operation) {
  if (code != SAMPIC256CH_Success) {
    throw std::runtime_error(std::string(operation) + " failed with SAMPIC error " +
                             std::to_string(static_cast<int>(code)));
  }
}

TriggerLogicParamStruct or_all_sampics_logic() {
  // The four FEB trigger inputs select SAMPICs 0..3. The L2 coincidence gate
  // is configured separately below; this tree forms the primitive OR.
  TriggerLogicParamStruct logic{};
  logic.SelInput0 = 0;
  logic.SelInput1 = 1;
  logic.SelInput2 = 2;
  logic.SelInput3 = 3;
  logic.Layer1TriggerLogic0 = LOGIC_OR;
  logic.Layer1TriggerLogic1 = LOGIC_OR;
  logic.Layer1TriggerLogic2 = LOGIC_OR;
  logic.Layer2TriggerLogic0 = LOGIC_OR;
  logic.Layer2TriggerLogic1 = LOGIC_OR;
  logic.Layer3TriggerLogic = LOGIC_OR;
  return logic;
}

class Session {
 public:
  explicit Session(const Config& config) : cfg_(config) {
    connection_.ConnectionType = UDP_CONNECTION;
    connection_.ControlBoardControlType = CTRL_AND_DAQ;
    std::snprintf(connection_.CtrlIpAddress, sizeof(connection_.CtrlIpAddress), "%s",
                  cfg_.ip.c_str());
    connection_.CtrlPort = cfg_.port;
    check(SAMPIC256CH_OpenCrateConnection(connection_, &info_), "OpenCrateConnection");
    connected_ = true;
    check(SAMPIC256CH_SetDefaultParameters(&info_, &params_), "SetDefaultParameters");
    if (cfg_.load_calibration) load_calibration();
    check(SAMPIC256CH_AllocateEventMemory(&event_buffer_, &frames_), "AllocateEventMemory");
  }

  ~Session() {
    if (running_) SAMPIC256CH_StopRun(&info_, &params_);
    if (event_buffer_ || frames_) SAMPIC256CH_FreeEventMemory(&event_buffer_, &frames_);
    if (connected_) SAMPIC256CH_CloseCrateConnection(&info_);
  }

  void configure(bool gate_enabled) {
    check(SAMPIC256CH_SetSamplingFrequency(&info_, &params_, cfg_.sampling_mhz, FALSE),
          "SetSamplingFrequency");

    check(SAMPIC256CH_SetChannelMode(&info_, &params_, cfg_.board, ALL_CHANNELs, FALSE),
          "DisableBoardChannels");
    check(SAMPIC256CH_SetSampicChannelTriggerMode(&info_, &params_, cfg_.board, ALL_SAMPICs,
                                                   ALL_CHANNELs, SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
          "SetSelfTriggerMode");
    for (const int channel : cfg_.channels) {
      check(SAMPIC256CH_SetChannelMode(&info_, &params_, cfg_.board, channel, TRUE),
            "EnableChannel");
      // SetChannelMode takes a FEB-wide channel number (0--63), but the
      // threshold API takes a SAMPIC index and its local channel (0--15).
      const int sampic = channel / NB_OF_CHANNELS_IN_SAMPIC;
      const int sampic_channel = channel % NB_OF_CHANNELS_IN_SAMPIC;
      check(SAMPIC256CH_SetSampicChannelInternalThreshold(&info_, &params_, cfg_.board, sampic,
                                                           sampic_channel,
                                                           cfg_.threshold_volts),
            "SetChannelThreshold");
    }

    check(SAMPIC256CH_SetExternalTriggerType(&info_, &params_, EXT_SIG),
          "SetExternalTriggerType");
    check(SAMPIC256CH_SetExternalTriggerEdge(&info_, &params_, cfg_.external_edge),
          "SetExternalTriggerEdge");
    check(SAMPIC256CH_SetExternalTriggerSigLevel(&info_, &params_, cfg_.external_level),
          "SetExternalTriggerSigLevel");
    check(SAMPIC256CH_SetExternalTriggerCounterMode(
              &info_, &params_, cfg_.enable_trigger_counter ? TRUE : FALSE,
              cfg_.detect_external_trigger_id ? TRUE : FALSE),
          "SetExternalTriggerCounterMode");
    check(SAMPIC256CH_SetMinNbOfTriggersPerEvent(&info_, &params_,
                                                 cfg_.trigger_records_per_frame),
          "SetMinNbOfTriggersPerEvent");

    check(SAMPIC256CH_SetLevel2TriggerBuildOption(&info_, &params_, TRUE),
          "SetLevel2TriggerBuildOption");
    check(SAMPIC256CH_SetSampicTriggerOption(&info_, &params_, cfg_.board, ALL_SAMPICs,
                                              SAMPIC_TRISSER_IS_FEB_GT),
          "SetSampicTriggerOption(L2)");
    check(SAMPIC256CH_SetFrontEndBoardGlobalTriggerOption(&info_, &params_, cfg_.board,
                                                           FEB_GLOBAL_TRIGGER_IS_L2),
          "SetFrontEndBoardGlobalTriggerOption(L2)");
    check(SAMPIC256CH_SetLevel2TriggerLogic(&info_, &params_, cfg_.board,
                                            or_all_sampics_logic()),
          "SetLevel2TriggerLogic");
    check(SAMPIC256CH_SetPrimitivesGateLength(&info_, &params_, cfg_.primitive_gate_clocks),
          "SetPrimitivesGateLength");
    check(SAMPIC256CH_SetLevel2LatencyGateLength(&info_, &params_, cfg_.latency_gate_clocks),
          "SetLevel2LatencyGateLength");
    check(SAMPIC256CH_SetLevel2ExtTrigGate(&info_, &params_, cfg_.board,
                                           cfg_.external_gate_clocks),
          "SetLevel2ExtTrigGate");
    check(SAMPIC256CH_SetLevel2CoincidenceModeWithExtTrigGate(
              &info_, &params_, cfg_.board, gate_enabled ? TRUE : FALSE),
          "SetLevel2CoincidenceModeWithExtTrigGate");
  }

  void print_effective_config(bool gate_enabled) const {
    std::cout << "L2 external-gate configuration applied\n"
              << "  FEB=" << cfg_.board << ", channels=";
    for (const int channel : cfg_.channels) std::cout << channel << ' ';
    std::cout << "\n  primitive gate=" << int(cfg_.primitive_gate_clocks * 10)
              << " ns, latency=" << int(cfg_.latency_gate_clocks * 10)
              << " ns, external gate=" << int(cfg_.external_gate_clocks * 10)
              << " ns\n  external coincidence=" << (gate_enabled ? "ENABLED" : "DISABLED")
              << ", trigger counter=" << (cfg_.enable_trigger_counter ? "ENABLED" : "DISABLED")
              << " (" << int(cfg_.trigger_records_per_frame) << " record(s)/frame)"
              << "\n";
  }

  void start() {
    check(SAMPIC256CH_StartRun(&info_, &params_, TRUE), "StartRun");
    running_ = true;
  }

  void stop() {
    if (!running_) return;
    check(SAMPIC256CH_StopRun(&info_, &params_), "StopRun");
    running_ = false;
  }

  bool read(EventStruct& event, int& hits) {
    check(SAMPIC256CH_PrepareEvent(&info_, &params_), "PrepareEvent");
    SAMPIC256CH_ErrCode code = SAMPIC256CH_NoFrameRead;
    int frames = 0;
    for (int loop = 0; loop <= cfg_.readout_max_loops; ++loop) {
      code = SAMPIC256CH_ReadEventBuffer(&info_, 0, event_buffer_, frames_, &frames);
      if (code == SAMPIC256CH_Success) {
        code = SAMPIC256CH_DecodeEvent(&info_, &params_, frames_, &event, frames, &hits);
      }
      if (code == SAMPIC256CH_Success) return true;
      if (code == SAMPIC256CH_AcquisitionError || code == SAMPIC256CH_ErrInvalidEvent ||
          code == SAMPIC256CH_ErrInvalidTriggerDataEvent) {
        throw std::runtime_error("Read/Decode event failed with SAMPIC error " +
                                 std::to_string(static_cast<int>(code)));
      }
      if (loop % 100 == 0) check(SAMPIC256CH_PrepareEvent(&info_, &params_), "PrepareEvent");
      if (cfg_.readout_retry_sleep_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(cfg_.readout_retry_sleep_us));
      }
    }
    return false;
  }

 private:
  void load_calibration() {
    std::array<char, MAX_PATHNAME_LENGTH> directory{};
    const auto absolute = std::filesystem::absolute(cfg_.calibration_dir).string();
    std::snprintf(directory.data(), directory.size(), "%s", absolute.c_str());
    const auto code = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info_, &params_, directory.data());
    if (code != SAMPIC256CH_Success) {
      std::cerr << "Warning: calibration load failed (SAMPIC error " << int(code) << ")\n";
    }
  }

  const Config& cfg_;
  CrateConnectionParamStruct connection_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* frames_ = nullptr;
  bool connected_ = false;
  bool running_ = false;
};

struct HitTime {
  int event = 0;
  int channel = 0;
  double time_ns = 0.0;
  double first_cell_time_ns = 0.0;
  int trigger_position_cell = 0;
};

double sampling_period_ns(const Config& cfg) {
  return 1000.0 / static_cast<double>(cfg.sampling_mhz);
}

void print_event(int index, const EventStruct& event, int hits) {
  std::cout << "event=" << index << " hits=" << hits
            << " trigger_records=" << event.TriggerData.NbOfTriggers << '\n';
  for (int i = 0; i < event.TriggerData.NbOfTriggers; ++i) {
    std::cout << "  trigger[" << i << "]: fpga_id=" << event.TriggerData.TriggerIDFromFPGA[i]
              << " ext_id=" << event.TriggerData.TriggerIDFromExtTrig[i]
              << " timestamp_ns=" << event.TriggerData.TriggerTimeStamp[i] << '\n';
  }
}

void print_correlation(const Config& cfg, const std::vector<HitTime>& hits,
                       std::vector<double> trigger_times) {
  if (hits.empty()) {
    std::cout << "Correlation: no hit timestamps were decoded.\n";
    return;
  }
  if (trigger_times.empty()) {
    std::cout << "Correlation: no external-trigger records were decoded; cannot test coincidence.\n";
    return;
  }

  std::sort(trigger_times.begin(), trigger_times.end());
  int matched = 0;
  double smallest_delta = std::numeric_limits<double>::infinity();
  double largest_delta = -std::numeric_limits<double>::infinity();
  double sum_delta = 0.0;
  double nearest_smallest_delta = std::numeric_limits<double>::infinity();
  double nearest_largest_delta = -std::numeric_limits<double>::infinity();
  double nearest_sum_delta = 0.0;
  std::vector<std::pair<HitTime, double>> unmatched;

  for (const auto& hit : hits) {
    const double target = hit.time_ns - cfg.expected_hit_minus_trigger_ns;
    const auto upper = std::lower_bound(trigger_times.begin(), trigger_times.end(), target);
    auto best = upper;
    if (upper == trigger_times.end() ||
        (upper != trigger_times.begin() &&
         std::abs(*(upper - 1) - target) < std::abs(*upper - target))) {
      best = upper - 1;
    }

    const double delta = hit.time_ns - *best;
    nearest_smallest_delta = std::min(nearest_smallest_delta, delta);
    nearest_largest_delta = std::max(nearest_largest_delta, delta);
    nearest_sum_delta += delta;
    if (std::abs(delta - cfg.expected_hit_minus_trigger_ns) <= cfg.correlation_tolerance_ns) {
      ++matched;
      smallest_delta = std::min(smallest_delta, delta);
      largest_delta = std::max(largest_delta, delta);
      sum_delta += delta;
    } else if (unmatched.size() < 5) {
      unmatched.emplace_back(hit, delta);
    }
  }

  std::cout << "Correlation: hits=" << hits.size() << " external_triggers=" << trigger_times.size()
            << " matched=" << matched << '/' << hits.size() << " (expected hit-trigger="
            << cfg.expected_hit_minus_trigger_ns << " ns, tolerance=+/-"
            << cfg.correlation_tolerance_ns << " ns)\n";
  std::cout << "  nearest hit-trigger delta: mean=" << (nearest_sum_delta / hits.size())
            << " ns, range=[" << nearest_smallest_delta << ", " << nearest_largest_delta
            << "] ns\n";
  if (matched == 0) {
    const double suggested_offset = nearest_sum_delta / hits.size();
    const double suggested_tolerance = std::max(
        10.0, std::max(std::abs(nearest_smallest_delta - suggested_offset),
                       std::abs(nearest_largest_delta - suggested_offset)) + 5.0);
    std::cout << "  observed alignment: set expected_hit_minus_trigger_ns to "
              << suggested_offset << " and use a starting tolerance of +/-"
              << suggested_tolerance << " ns, then rerun to count matches.\n";
  }
  if (matched > 0) {
    std::cout << "  matched hit-trigger delta: mean=" << (sum_delta / matched)
              << " ns, range=[" << smallest_delta << ", " << largest_delta << "] ns\n";
  }
  for (const auto& [hit, delta] : unmatched) {
    std::cout << "  unmatched hit: event=" << hit.event << " channel=" << hit.channel
              << " trigger_cell_timestamp_ns=" << hit.time_ns
              << " first_cell_timestamp_ns=" << hit.first_cell_time_ns
              << " trigger_position_cell=" << hit.trigger_position_cell
              << " nearest_delta=" << delta << " ns\n";
  }
}

struct Measurement {
  std::string label;
  int events = 0;
  int timeouts = 0;
  std::size_t hits = 0;
  std::size_t trigger_records = 0;
  double duration_s = 0.0;
  std::map<int, std::size_t> channel_hits;
};

Measurement run_measurement(const Config& cfg, bool gate_enabled, std::string label = {}) {
  Measurement result;
  result.label = label.empty() ? (gate_enabled ? "gated" : "ungated") : std::move(label);

  // Both vendor structures are large (the event contains the full waveform
  // buffer), so keep them off the limited process stack.
  auto session = std::make_unique<Session>(cfg);
  auto event = std::make_unique<EventStruct>();
  session->configure(gate_enabled);
  session->print_effective_config(gate_enabled);
  session->start();
  const auto begin = std::chrono::steady_clock::now();
  const auto deadline = begin + std::chrono::duration<double>(cfg.comparison_duration_s);

  while (std::chrono::steady_clock::now() < deadline) {
    *event = EventStruct{};
    int hits = 0;
    if (!session->read(*event, hits)) {
      ++result.timeouts;
      continue;
    }
    ++result.events;
    result.hits += static_cast<std::size_t>(hits);
    result.trigger_records += static_cast<std::size_t>(event->TriggerData.NbOfTriggers);
    for (int i = 0; i < event->NbOfHitsInEvent; ++i) {
      ++result.channel_hits[event->Hit[i].Channel];
    }
  }
  session->stop();
  result.duration_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
  return result;
}

class LecroyChannelStateGuard {
 public:
  explicit LecroyChannelStateGuard(const Config& cfg) : channel_(cfg.lecroy_gate_channel) {
    client_.Connect(cfg.lecroy_ip, cfg.lecroy_port);
    original_disabled_ = client_.IsChannelDisabled(channel_);
  }

  ~LecroyChannelStateGuard() {
    if (!restore_needed_) return;
    try {
      client_.SetChannelDisabled(channel_, original_disabled_);
    } catch (const std::exception& error) {
      std::cerr << "Warning: failed to restore Lecroy " << channel_ << " DISA state: "
                << error.what() << '\n';
    }
  }

  void set_disabled(bool disabled) {
    client_.SetChannelDisabled(channel_, disabled);
    restore_needed_ = true;
    const bool actual = client_.IsChannelDisabled(channel_);
    if (actual != disabled) {
      throw std::runtime_error("Lecroy " + channel_ + " DISA readback did not match request");
    }
  }

 private:
  sampic::lecroy::LecroyClient client_;
  std::string channel_;
  bool original_disabled_ = false;
  bool restore_needed_ = false;
};

double rate(std::size_t count, double duration_s) {
  return duration_s > 0.0 ? static_cast<double>(count) / duration_s : 0.0;
}

double per_trigger(std::size_t count, std::size_t trigger_records) {
  return trigger_records > 0 ? static_cast<double>(count) / trigger_records : 0.0;
}

void print_measurement_comparison(const Measurement& ungated, const Measurement& gated) {
  std::cout << "\nL2 gate comparison\n==================\n";
  for (const auto* result : {&ungated, &gated}) {
    std::cout << result->label << ": duration=" << std::fixed << std::setprecision(3)
              << result->duration_s << " s, events=" << result->events << " ("
              << rate(result->events, result->duration_s) << "/s), hits=" << result->hits << " ("
              << rate(result->hits, result->duration_s) << "/s), external trigger records="
              << result->trigger_records << " (" << rate(result->trigger_records, result->duration_s)
              << "/s), events/trigger=" << per_trigger(result->events, result->trigger_records)
              << ", hits/trigger=" << per_trigger(result->hits, result->trigger_records)
              << ", read timeouts=" << result->timeouts << '\n';
  }

  std::map<int, std::size_t> channels = ungated.channel_hits;
  for (const auto& [channel, count] : gated.channel_hits) channels[channel] += count;
  if (channels.empty()) {
    std::cout << "No hit channels were observed in either measurement.\n";
    return;
  }

  std::cout << "\nFEB channel    ungated hits/s    gated hits/s    rate ratio"
               "    ungated hit/trigger    gated hit/trigger    trigger-normalized ratio\n";
  for (const auto& [channel, ignored] : channels) {
    const auto baseline = ungated.channel_hits.contains(channel) ? ungated.channel_hits.at(channel) : 0;
    const auto enabled = gated.channel_hits.contains(channel) ? gated.channel_hits.at(channel) : 0;
    const double baseline_rate = rate(baseline, ungated.duration_s);
    const double gated_rate = rate(enabled, gated.duration_s);
    const double baseline_per_trigger = per_trigger(baseline, ungated.trigger_records);
    const double gated_per_trigger = per_trigger(enabled, gated.trigger_records);
    std::cout << "Ch " << std::setw(2) << channel << "       "
              << std::setw(14) << std::setprecision(3) << baseline_rate << "  "
              << std::setw(14) << gated_rate << "  ";
    if (baseline_rate > 0.0) {
      std::cout << std::setw(14) << (gated_rate / baseline_rate);
    } else {
      std::cout << std::setw(14) << "n/a";
    }
    std::cout << "  " << std::setw(20) << baseline_per_trigger << "  "
              << std::setw(18) << gated_per_trigger << "  ";
    if (baseline_per_trigger > 0.0) {
      std::cout << std::setw(24) << (gated_per_trigger / baseline_per_trigger);
    } else {
      std::cout << std::setw(24) << "n/a";
    }
    std::cout << '\n';
  }
}

void print_b_output_gate_test(const Measurement& b_enabled, const Measurement& gated_b_disabled,
                              const Measurement& ungated_b_disabled) {
  std::cout << "\nLecroy-B output gate-control test\n================================\n";
  for (const auto* result : {&b_enabled, &gated_b_disabled, &ungated_b_disabled}) {
    std::cout << result->label << ": events/s=" << std::fixed << std::setprecision(3)
              << rate(result->events, result->duration_s) << ", hits/s="
              << rate(result->hits, result->duration_s) << ", trigger records/s="
              << rate(result->trigger_records, result->duration_s) << '\n';
  }
  std::cout << "Expected: gated+B-disabled is near zero/background, while ungated+B-disabled "
               "returns to the self-trigger rate.\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parse_args(argc, argv);
    const Config config = load_config(options.config_path);
    if (!options.apply) {
      std::cout << "Validated " << options.config_path
                << ". No hardware settings were changed; re-run with --apply.\n";
      return 0;
    }

    if (options.compare) {
      std::cout << "Running ungated baseline for " << config.comparison_duration_s
                << " s, then gated measurement for the same duration.\n";
      const Measurement ungated = run_measurement(config, false);
      const Measurement gated = run_measurement(config, true);
      print_measurement_comparison(ungated, gated);
      return gated.events > 0 ? 0 : 2;
    }

    if (options.b_output_gate_test) {
      std::cout << "B-output gate-control test: each phase lasts " << config.comparison_duration_s
                << " s. The original Lecroy " << config.lecroy_gate_channel
                << " output state will be restored automatically.\n";
      LecroyChannelStateGuard lecroy_state(config);
      lecroy_state.set_disabled(false);
      const Measurement gated_b_enabled = run_measurement(config, true, "gated, B enabled");
      lecroy_state.set_disabled(true);
      const Measurement gated_b_disabled = run_measurement(config, true, "gated, B disabled");
      const Measurement ungated_b_disabled =
          run_measurement(config, false, "ungated, B disabled");
      print_b_output_gate_test(gated_b_enabled, gated_b_disabled, ungated_b_disabled);
      return (gated_b_enabled.events > 0 && ungated_b_disabled.events > 0) ? 0 : 2;
    }

    Session session(config);
    const bool gate_enabled = !options.without_gate;
    session.configure(gate_enabled);
    session.print_effective_config(gate_enabled);
    session.start();

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::duration<double>(config.duration_s);
    int received = 0;
    int timeouts = 0;
    std::vector<HitTime> hit_times;
    std::vector<double> trigger_times;
    const double sample_period_ns = sampling_period_ns(config);
    while (received < config.max_events && std::chrono::steady_clock::now() < deadline) {
      EventStruct event{};
      int hits = 0;
      if (!session.read(event, hits)) {
        ++timeouts;
        continue;
      }
      const int event_index = ++received;
      print_event(event_index, event, hits);
      for (int i = 0; i < event.NbOfHitsInEvent; ++i) {
        const auto& hit = event.Hit[i];
        // TriggerData is timestamped at the external input edge.  The waveform's
        // FirstCellTimeStamp is the beginning of its 64-cell capture window, so
        // move it to the decoder's internal trigger-cell position before comparing
        // it to an external-trigger timestamp.
        const double trigger_cell_time_ns =
            hit.FirstCellTimeStamp + hit.AdvancedParams.FirstTriggerPositionCell * sample_period_ns;
        hit_times.push_back(HitTime{event_index, hit.Channel, trigger_cell_time_ns,
                                    hit.FirstCellTimeStamp,
                                    hit.AdvancedParams.FirstTriggerPositionCell});
      }
      for (int i = 0; i < event.TriggerData.NbOfTriggers; ++i) {
        trigger_times.push_back(event.TriggerData.TriggerTimeStamp[i]);
      }
    }
    std::cout << "Summary: events=" << received << " read_timeouts=" << timeouts << '\n';
    print_correlation(config, hit_times, std::move(trigger_times));
    return received > 0 ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "l2_external_gate_probe: " << error.what() << '\n';
    return 1;
  }
}
