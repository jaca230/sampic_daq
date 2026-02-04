#include "sampic_tests/modes/double_pulse/config.h"
#include "sampic_tests/modes/double_pulse/sampic_session.h"
#include "sampic_tests/lecroy/lecroy_client.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
  std::string config_path;
  double duration_s = 3.0;
  int max_hits = 20;
  int combo_index = 0;
  bool use_lecroy = true;
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
    } else if (arg == "--duration") {
      opts.duration_s = std::stod(require_value(arg));
    } else if (arg == "--max-hits") {
      opts.max_hits = std::stoi(require_value(arg));
    } else if (arg == "--combo") {
      opts.combo_index = std::stoi(require_value(arg));
    } else if (arg == "--no-lecroy") {
      opts.use_lecroy = false;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: double_pulse_smoke --config <file> [options]\n"
                << "  --duration <sec>   Sample duration (default 3.0)\n"
                << "  --max-hits <n>     Max hits to print (default 20)\n"
                << "  --combo <idx>      Combo index to test (default 0)\n"
                << "  --no-lecroy        Skip configuring Lecroy\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown option: " + std::string(arg));
    }
  }

  if (opts.config_path.empty()) {
    throw std::runtime_error("Missing --config <file>");
  }
  if (opts.duration_s <= 0.0) {
    throw std::runtime_error("--duration must be > 0");
  }
  if (opts.max_hits < 0) {
    throw std::runtime_error("--max-hits must be >= 0");
  }
  if (opts.combo_index < 0) {
    throw std::runtime_error("--combo must be >= 0");
  }
  return opts;
}

void print_hits(const std::vector<sampic::scan::HitRecord>& hits, int max_hits) {
  const int limit = std::min<int>(max_hits, hits.size());
  for (int i = 0; i < limit; ++i) {
    const auto& hit = hits[i];
    std::cout << "  hit[" << i << "] feb=" << hit.board
              << " sampic=" << hit.sampic
              << " ch=" << hit.channel
              << " amp=" << hit.amplitude
              << " base=" << hit.baseline
              << " tot=" << hit.tot_ns
              << " ts=" << hit.first_cell_ts_ns
              << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto opts = parse_args(argc, argv);
    auto cfg = sampic::double_pulse::load_double_pulse_config(opts.config_path);
    auto combos = sampic::double_pulse::build_parameter_space(cfg.scan);

    if (opts.combo_index >= static_cast<int>(combos.size())) {
      throw std::runtime_error("combo index out of range (max " +
                               std::to_string(combos.size() - 1) + ")");
    }
    auto combo = combos[opts.combo_index];

    std::cout << "Combo " << opts.combo_index << ": freq=" << combo.lecroy_frequency_hz
              << "Hz rate=" << combo.digitizer_rate_mhz
              << "MHz auto_conv=" << (combo.auto_conversion ? "on" : "off")
              << " amp=" << combo.lecroy_amplitude_v
              << "V channels=" << combo.channels.size();
    if (!combo.channel_label.empty()) {
      std::cout << " (" << combo.channel_label << ")";
    }
    std::cout << "\n";

    if (opts.use_lecroy) {
      sampic::lecroy::LecroyClient lecroy;
      lecroy.Configure(cfg.lecroy);
      if (combo.lecroy_frequency_hz > 0.0) {
        lecroy.SetFrequency(combo.lecroy_frequency_hz);
      }
      if (combo.lecroy_amplitude_v > 0.0) {
        lecroy.SetAmplitude(combo.lecroy_amplitude_v);
      }
    }

    sampic::double_pulse::SampicSession session(cfg.connection, cfg.external_trigger);
    session.configure_for_combo(combo, cfg.scan.board_index, combo.channels);
    session.set_threshold(cfg.scan.board_index, combo.threshold_volts);
    std::cout << "Configured threshold: " << combo.threshold_volts << " V\n";

    const auto report_enabled = [&](const std::string& label) {
      try {
        const auto readback = session.readback_settings(cfg.scan.board_index);
        std::cout << label << " enabled channels (" << readback.enabled_channels.size()
                  << "):";
        for (int ch : readback.enabled_channels) {
          std::cout << " " << ch;
        }
        std::cout << "\n";
      } catch (const std::exception& ex) {
        std::cout << label << " readback failed: " << ex.what() << "\n";
      }
    };

    report_enabled("Configured");

    std::vector<std::string> start_errors;
    int attempts = 0;
    if (!session.start_run_with_retry(cfg.start_retry, attempts, start_errors)) {
      std::cout << "Run failed to start after " << attempts << " attempts\n";
      for (const auto& err : start_errors) {
        std::cout << "  " << err << "\n";
      }
      return 1;
    }

    std::vector<sampic::scan::HitRecord> hits;
    const auto stats = session.acquire_sample(cfg.readout, opts.duration_s, nullptr, true, &hits);
    session.stop_run();

    std::cout << "Configured-only run:\n";
    std::cout << "  Events: " << stats.events
              << " hits=" << stats.total_hits
              << " retries=" << stats.retries
              << " decode_errors=" << stats.decode_errors
              << " duration=" << stats.duration_s << "s\n";
    if (!stats.error_messages.empty()) {
      std::cout << "  Errors:\n";
      for (const auto& err : stats.error_messages) {
        std::cout << "    " << err << "\n";
      }
    }
    if (!hits.empty() && opts.max_hits > 0) {
      std::cout << "  First " << std::min<int>(opts.max_hits, hits.size()) << " hits:\n";
      print_hits(hits, opts.max_hits);
    } else {
      std::cout << "  No hits captured.\n";
    }

    // Second pass: enable all channels on the board for comparison.
    std::vector<int> all_channels;
    all_channels.reserve(NB_OF_CHANNELS_IN_FE_BOARD);
    for (int ch = 0; ch < NB_OF_CHANNELS_IN_FE_BOARD; ++ch) {
      all_channels.push_back(ch);
    }
    session.configure_for_combo(combo, cfg.scan.board_index, all_channels);
    session.set_threshold(cfg.scan.board_index, combo.threshold_volts);
    std::cout << "All-channels threshold: " << combo.threshold_volts << " V\n";
    report_enabled("All-channels");

    std::vector<std::string> start_errors_all;
    int attempts_all = 0;
    if (!session.start_run_with_retry(cfg.start_retry, attempts_all, start_errors_all)) {
      std::cout << "All-channels run failed to start after " << attempts_all << " attempts\n";
      for (const auto& err : start_errors_all) {
        std::cout << "  " << err << "\n";
      }
      return 1;
    }

    hits.clear();
    const auto stats_all = session.acquire_sample(cfg.readout, opts.duration_s, nullptr, true, &hits);
    session.stop_run();

    std::cout << "All-channels run:\n";
    std::cout << "  Events: " << stats_all.events
              << " hits=" << stats_all.total_hits
              << " retries=" << stats_all.retries
              << " decode_errors=" << stats_all.decode_errors
              << " duration=" << stats_all.duration_s << "s\n";
    if (!stats_all.error_messages.empty()) {
      std::cout << "  Errors:\n";
      for (const auto& err : stats_all.error_messages) {
        std::cout << "    " << err << "\n";
      }
    }
    if (!hits.empty() && opts.max_hits > 0) {
      std::cout << "  First " << std::min<int>(opts.max_hits, hits.size()) << " hits:\n";
      print_hits(hits, opts.max_hits);
    } else {
      std::cout << "  No hits captured.\n";
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}
