#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "sampic_tests/lecroy/lecroy_client.h"

namespace {

struct Options {
  std::string config_path;
  double override_delay_ns = -1.0;
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
    } else if (arg == "--delay-ns") {
      opts.override_delay_ns = std::stod(require_value(arg));
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: lecroy_test --config <file> [--delay-ns value]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }
  if (opts.config_path.empty()) {
    throw std::runtime_error("lecroy_test requires --config <file>");
  }
  return opts;
}

sampic::lecroy::LecroyConfig load_config(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Unable to open config file: " + path);
  }
  const auto json = nlohmann::json::parse(in, nullptr, true, true);
  sampic::lecroy::LecroyConfig cfg;
  if (json.contains("lecroy")) {
    const auto& node = json.at("lecroy");
    if (node.contains("ip")) cfg.ip = node.at("ip").get<std::string>();
    if (node.contains("port")) cfg.port = node.at("port").get<int>();
    if (node.contains("frequency_hz")) cfg.frequency_hz = node.at("frequency_hz").get<double>();
    if (node.contains("trigger_mode")) cfg.trigger_mode = node.at("trigger_mode").get<std::string>();
    if (node.contains("trigger_slope")) cfg.trigger_slope = node.at("trigger_slope").get<std::string>();
    if (node.contains("trigger_level_volts")) cfg.trigger_level_volts = node.at("trigger_level_volts").get<double>();
    if (node.contains("initial_delay_ns")) cfg.initial_delay_ns = node.at("initial_delay_ns").get<double>();
    if (node.contains("manual_trigger")) cfg.manual_trigger = node.at("manual_trigger").get<bool>();
    if (node.contains("settle_delay_s")) cfg.settle_delay_s = node.at("settle_delay_s").get<double>();
    if (node.contains("channel")) {
      cfg.channel.channel = node.at("channel").get<std::string>();
    }
    if (node.contains("amplitude_v")) cfg.channel.amplitude_v = node.at("amplitude_v").get<double>();
    if (node.contains("baseline_v")) cfg.channel.baseline_v = node.at("baseline_v").get<double>();
    if (node.contains("width_ns")) cfg.channel.width_ns = node.at("width_ns").get<double>();
    if (node.contains("lead_ns")) cfg.channel.lead_ns = node.at("lead_ns").get<double>();
    if (node.contains("trail_ns")) cfg.channel.trail_ns = node.at("trail_ns").get<double>();
  }
  if (json.contains("lecroy_channel")) {
    const auto& ch = json.at("lecroy_channel");
    if (ch.contains("channel")) cfg.channel.channel = ch.at("channel").get<std::string>();
    if (ch.contains("amplitude_v")) cfg.channel.amplitude_v = ch.at("amplitude_v").get<double>();
    if (ch.contains("baseline_v")) cfg.channel.baseline_v = ch.at("baseline_v").get<double>();
    if (ch.contains("width_ns")) cfg.channel.width_ns = ch.at("width_ns").get<double>();
    if (ch.contains("lead_ns")) cfg.channel.lead_ns = ch.at("lead_ns").get<double>();
    if (ch.contains("trail_ns")) cfg.channel.trail_ns = ch.at("trail_ns").get<double>();
    if (ch.contains("output_main")) cfg.channel.output_main = ch.at("output_main").get<bool>();
    if (ch.contains("output_inverse")) cfg.channel.output_inverse = ch.at("output_inverse").get<bool>();
    if (ch.contains("double_pulse_enabled")) cfg.channel.double_pulse_enabled = ch.at("double_pulse_enabled").get<bool>();
  }
  return cfg;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);
    auto cfg = load_config(opts.config_path);
    sampic::lecroy::LecroyClient client;
    client.Configure(cfg);
    if (opts.override_delay_ns > 0.0) {
      client.SetDoublePulseDelay(opts.override_delay_ns);
    }
    std::cout << "Lecroy configuration applied successfully.\n";
    const auto name = client.Query("*IDN?");
    std::cout << "Instrument ID: " << name << "\n";

    auto try_query = [&](const std::string& cmd) {
      try {
        return client.Query(cmd);
      } catch (const std::exception& ex) {
        std::cerr << "Query '" << cmd << "' failed: " << ex.what() << "\n";
        return std::string("<error>");
      }
    };
    auto try_agilent = [&](const std::string& cmd) {
      try {
        return client.Query(cmd);
      } catch (const std::exception&) {
        return std::string("<error>");
      }
    };
    const bool looks_like_agilent = name.find("33250A") != std::string::npos;
    std::cout << "Readback:\n";
    if (looks_like_agilent) {
      std::vector<std::string> agilent_cmds = {
          "FUNC?", "FREQ?", "VOLT?", "VOLT:OFFS?", "VOLT:HIGH?", "VOLT:LOW?",
          "PULS:WIDT?", "PULS:DEL?", "OUTP?", "OUTP:LOAD?", "TRIG:SOUR?"};
      for (const auto& cmd : agilent_cmds) {
        std::cout << "  " << cmd << " = " << try_agilent(cmd) << "\n";
      }
    } else {
      const auto freq = try_query("FREQ?");
      const auto dbl = try_query(cfg.channel.channel + ":DBL?");
      const auto amp = try_query(cfg.channel.channel + ":AMP?");
      const auto base = try_query(cfg.channel.channel + ":BASE?");
      const auto wid = try_query(cfg.channel.channel + ":WID?");
      const auto delay = try_query(cfg.channel.channel + ":DEL?");
      const auto disa = try_query(cfg.channel.channel + ":DISA?");
      std::cout << "  FREQ?      = " << freq << "\n"
                << "  " << cfg.channel.channel << ":DBL?  = " << dbl << "\n"
                << "  " << cfg.channel.channel << ":AMP?  = " << amp << "\n"
                << "  " << cfg.channel.channel << ":BASE? = " << base << "\n"
                << "  " << cfg.channel.channel << ":WID?  = " << wid << "\n"
                << "  " << cfg.channel.channel << ":DEL?  = " << delay << "\n"
                << "  " << cfg.channel.channel << ":DISA? = " << disa << "\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "lecroy_test error: " << ex.what() << "\n";
    return 1;
  }
}
