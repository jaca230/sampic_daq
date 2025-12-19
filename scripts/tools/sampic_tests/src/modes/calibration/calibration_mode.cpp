#include "sampic_tests/modes/calibration/calibration_mode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

namespace {

using Options = sampic::calibration::CalibrationCheckOptions;
namespace fs = std::filesystem;

void check(SAMPIC256CH_ErrCode err, std::string_view what) {
  if (err != SAMPIC256CH_Success) {
    throw std::runtime_error(std::string(what) + " failed (code " +
                             std::to_string(static_cast<int>(err)) + ")");
  }
}

void configure_self_trigger(CrateInfoStruct& info,
                            CrateParamStruct& params,
                            const Options& opts,
                            int board_index) {
  check(SAMPIC256CH_SetChannelMode(&info, &params, board_index, ALL_CHANNELs, TRUE),
        "SetChannelMode");
  check(SAMPIC256CH_SetSampicChannelTriggerMode(&info, &params, board_index, ALL_SAMPICs,
                                                ALL_CHANNELs, SAMPIC_CHANNEL_SELF_TRIGGER_MODE),
        "SetTriggerMode");
  check(SAMPIC256CH_SetChannelSelflTriggerEdge(&info, &params, board_index, ALL_SAMPICs,
                                               ALL_CHANNELs, RISING_EDGE),
        "SetTriggerEdge");
  check(SAMPIC256CH_SetSampicChannelPulseMode(&info, &params, board_index, ALL_SAMPICs,
                                              ALL_CHANNELs, TRUE),
        "SetPulseMode");
  check(SAMPIC256CH_SetSampicChannelInternalThreshold(
            &info, &params, board_index, ALL_SAMPICs, ALL_CHANNELs,
            static_cast<float>(opts.threshold)),
        "SetThreshold");
}

void print_hit(const HitStruct& hit, int max_samples) {
  std::cout << "  FEB=" << hit.FeBoardIndex << " Sampic=" << hit.SampicIndex
            << " Channel=" << hit.Channel << "\n";
  std::cout << "    Corrections: ADC=" << (hit.ADCCorrected ? "yes" : "no")
            << " INL=" << (hit.INLCorrected ? "yes" : "no")
            << " ResidualPed=" << (hit.ResidualPedestalCorrected ? "yes" : "no") << "\n";
  std::cout << "    Amplitude=" << hit.Amplitude << " Baseline=" << hit.Baseline
            << " TOT(ns)=" << hit.TOTValue
            << " FirstCellTS(ns)=" << hit.FirstCellTimeStamp << "\n";
  const int limit = std::clamp(max_samples, 1, MAX_NB_OF_SAMPLES);
  std::cout << "    Samples (corrected V | raw ADC):\n";
  for (int i = 0; i < limit; ++i) {
    std::cout << "      [" << std::setw(3) << i << "] "
              << std::fixed << std::setprecision(6) << hit.CorrectedDataSamples[i]
              << " | " << hit.OrderedRawDataSamples[i] << "\n";
  }
}

struct MissingFileEntry {
  std::string target;
  fs::path path;
};

struct CategoryReport {
  std::string name;
  std::size_t total = 0;
  std::vector<MissingFileEntry> missing;
};

void record_file(CategoryReport& report,
                 const std::string& target,
                 const fs::path& path,
                 bool exists) {
  ++report.total;
  if (!exists) {
    report.missing.push_back(MissingFileEntry{target, path});
  }
}

std::string board_stub(const FeBoardInfoStruct& fe,
                       std::string_view typed_prefix,
                       std::string_view fallback_prefix) {
  std::ostringstream oss;
  if (fe.FeBoardTypeCharFromEEPROM != 'Z' && fe.FeBoardTypeCharFromEEPROM != '\0') {
    oss << typed_prefix << fe.FeBoardTypeCharFromEEPROM << "_V" << fe.BoardVersion
        << "." << fe.BoardSerNum;
  } else {
    oss << fallback_prefix << fe.BoardVersion << "." << fe.BoardSerNum;
  }
  return oss.str();
}

std::string board_descriptor(int feb_index, const FeBoardInfoStruct& fe) {
  std::ostringstream oss;
  oss << "FEB " << feb_index << " (ver " << fe.BoardVersion << "." << fe.BoardSerNum;
  if (fe.FeBoardTypeCharFromEEPROM != 'Z' && fe.FeBoardTypeCharFromEEPROM != '\0') {
    oss << ", type " << fe.FeBoardTypeCharFromEEPROM;
  }
  oss << ")";
  return oss.str();
}

void summarize_category(const CategoryReport& report) {
  constexpr std::size_t kMaxExamples = 8;
  if (report.missing.empty()) {
    std::cout << "  [OK] " << report.name << ": located " << report.total << " file(s).\n";
    return;
  }
  std::cout << "  [MISS] " << report.name << ": missing " << report.missing.size()
            << " of " << report.total << " expected file(s).\n";
  const auto to_show = std::min(report.missing.size(), kMaxExamples);
  for (std::size_t i = 0; i < to_show; ++i) {
    const auto& miss = report.missing[i];
    std::cout << "    - " << miss.target << " → " << miss.path.string() << "\n";
  }
  if (report.missing.size() > kMaxExamples) {
    std::cout << "    ... plus " << (report.missing.size() - kMaxExamples)
              << " additional missing entries.\n";
  }
}

void explain_calibration_requirements(const CrateInfoStruct& info,
                                      const CrateParamStruct& params,
                                      const fs::path& base_dir) {
  std::cout << "Calibration directory resolved to: " << base_dir << "\n";
  std::cout << "Detected " << info.NbOfFeBoards << " FE board(s); "
            << "FreqEch=" << params.CommonParams.FreqEch
            << " MHz, ADC bits=" << params.CommonParams.ADCNbOfBits << ".\n";

  CategoryReport adc_ramp{"ADC ramp"};
  CategoryReport adc_linearity{"ADC linearity"};
  CategoryReport time_inl{"Time INL"};
  CategoryReport tot{"TOT"};
  CategoryReport trig_default{"Internal trigger (default)"};
  CategoryReport trig_baseline{"Internal trigger (baseline-specific)"};
  CategoryReport residual{"Residual pedestal"};

  for (int feb = 0; feb < info.NbOfFeBoards; ++feb) {
    const auto& fe = info.CrateBoardsInfo.FeBoardInfo[feb];
    std::cout << "  • " << board_descriptor(feb, fe) << "\n";

    for (int sampic = 0; sampic < NB_OF_SAMPICS_IN_FE_BOARD; ++sampic) {
      const auto ramp_primary = base_dir / "ADC_IRamp_Files" /
          (board_stub(fe, "ADC_IRamp_Calib_Board_T", "ADC_IRamp_Calib_Board_V") +
           "_feIndex_" + std::to_string(sampic) + "_FreqEch_" +
           std::to_string(params.CommonParams.FreqEch) + "_MS_s.txt");
      const auto ramp_fallback = base_dir / "ADC_IRamp_Files" /
          ("ADC_IRamp_Calib_Board_V" + std::to_string(fe.BoardVersion) + "." +
           std::to_string(fe.BoardSerNum) + "_feIndex_" + std::to_string(sampic) + ".txt");
      const bool ramp_primary_exists = fs::exists(ramp_primary);
      const bool ramp_fallback_exists = fs::exists(ramp_fallback);
      const bool ramp_exists = ramp_primary_exists || ramp_fallback_exists;
      const fs::path ramp_report_path =
          ramp_primary_exists ? ramp_primary
                              : (ramp_fallback_exists ? ramp_fallback : ramp_primary);
      record_file(adc_ramp,
                  "FEB " + std::to_string(feb) + " Sampic " + std::to_string(sampic),
                  ramp_report_path,
                  ramp_exists);

      const double baseline =
          params.FeBoardParams[feb].FeFpgaParams[sampic].SAMPICIndividualParams.VBaseline;
      std::ostringstream baseline_dir;
      baseline_dir << "Baseline_" << std::fixed << std::setprecision(3) << baseline;
      const auto trig_baseline_path = base_dir / "InternalTriggerThresholdOffsets_Files" /
          baseline_dir.str() /
          (board_stub(fe,
                      "IntTriggerThreshold_Calib_Board_T",
                      "IntTriggerThreshold_Calib_Board_V") +
           "_sampic_" + std::to_string(sampic) + ".txt");
      record_file(trig_baseline,
                  "FEB " + std::to_string(feb) + " Sampic " + std::to_string(sampic),
                  trig_baseline_path,
                  fs::exists(trig_baseline_path));
    }

    const auto trig_default_path = base_dir / "InternalTriggerThresholdOffsets_Files" /
        (board_stub(fe,
                    "IntTriggerThreshold_Calib_Board_T",
                    "IntTriggerThreshold_Calib_Board_V") + ".txt");
    record_file(trig_default,
                "FEB " + std::to_string(feb),
                trig_default_path,
                fs::exists(trig_default_path));

    for (int channel = 0; channel < NB_OF_CHANNELS_IN_FE_BOARD; ++channel) {
      const auto adc_lin_path = base_dir / "ADC_Linearity_Files" /
          (board_stub(fe,
                      "ADC_Linearity_Calib_Board_T",
                      "ADC_Linearity_Calib_Board_V") +
           "_FreqEch_" + std::to_string(params.CommonParams.FreqEch) +
           "_MS_s_ADCNbOfBits" + std::to_string(params.CommonParams.ADCNbOfBits) +
           "_Ch" + std::to_string(channel) + ".txt");
      record_file(adc_linearity,
                  "FEB " + std::to_string(feb) + " Ch " + std::to_string(channel),
                  adc_lin_path,
                  fs::exists(adc_lin_path));

      const auto inl_path = base_dir / "INL_Files" /
          (board_stub(fe,
                      "INL_Calib_Board_T",
                      "INL_Calib_Board_V") +
           "_FreqEch_" + std::to_string(params.CommonParams.FreqEch) +
           "_MS_s_Ch" + std::to_string(channel) + ".txt");
      record_file(time_inl,
                  "FEB " + std::to_string(feb) + " Ch " + std::to_string(channel),
                  inl_path,
                  fs::exists(inl_path));

      const auto tot_path = base_dir / "TOT_CalibFiles" /
          (board_stub(fe,
                      "TOT_IRamp_Calib_Board_T",
                      "TOT_IRamp_Calib_Board_V") +
           "_FreqEch_" + std::to_string(params.CommonParams.FreqEch) +
           "_MS_s_ADCNbOfBits" + std::to_string(params.CommonParams.ADCNbOfBits) +
           "_Ch" + std::to_string(channel) + ".txt");
      record_file(tot,
                  "FEB " + std::to_string(feb) + " Ch " + std::to_string(channel),
                  tot_path,
                  fs::exists(tot_path));

      const int sampic_for_channel = channel / NB_OF_CHANNELS_IN_SAMPIC;
      const double baseline =
          params.FeBoardParams[feb]
              .FeFpgaParams[sampic_for_channel]
              .SAMPICIndividualParams.VBaseline;
      std::ostringstream baseline_dir;
      baseline_dir << "Baseline_" << std::fixed << std::setprecision(3) << baseline;
      const auto resid_path = base_dir / "ResidualPedestal_Files" / baseline_dir.str() /
          (board_stub(fe,
                      "ResidualPedestals_Calib_Board_T",
                      "ResidualPedestals_Calib_Board_V") +
           "_Ch" + std::to_string(channel) + ".txt");
      record_file(residual,
                  "FEB " + std::to_string(feb) + " Ch " + std::to_string(channel),
                  resid_path,
                  fs::exists(resid_path));
    }
  }

  summarize_category(adc_ramp);
  summarize_category(adc_linearity);
  summarize_category(time_inl);
  summarize_category(tot);
  summarize_category(trig_default);
  summarize_category(trig_baseline);
  summarize_category(residual);
}

const fs::path& source_project_root() {
  static const fs::path root = [] {
    fs::path p{__FILE__};
    for (int i = 0; i < 4 && p.has_parent_path(); ++i) {
      p = p.parent_path();
    }
    return fs::weakly_canonical(p);
  }();
  return root;
}

}  // namespace

namespace sampic::calibration {

CalibrationMode::CalibrationMode(volatile std::sig_atomic_t* stop_flag)
    : stop_flag_(stop_flag) {}

std::string CalibrationMode::name() const {
  return "calibration";
}

std::string CalibrationMode::description() const {
  return "Capture a single self-trigger hit and display calibration state";
}

int CalibrationMode::run(int argc, char** argv) {
  const auto opts = parse_args(argc, argv);

  CrateConnectionParamStruct conn{};
  conn.ConnectionType = UDP_CONNECTION;
  conn.ControlBoardControlType = CTRL_AND_DAQ;
  std::snprintf(conn.CtrlIpAddress, sizeof(conn.CtrlIpAddress), "%s", opts.ip.c_str());
  conn.CtrlPort = opts.port;

  CrateInfoStruct info{};
  CrateParamStruct params{};
  check(SAMPIC256CH_OpenCrateConnection(conn, &info), "OpenCrateConnection");
  check(SAMPIC256CH_SetDefaultParameters(&info, &params), "SetDefaultParameters");

  void* event_buffer = nullptr;
  ML_Frame* ml_frames = nullptr;
  check(SAMPIC256CH_AllocateEventMemory(&event_buffer, &ml_frames), "AllocateEventMemory");

  if (opts.load_calibration) {
    fs::path dir{opts.calibration_dir};
    if (!dir.is_absolute()) {
      try {
        dir = fs::weakly_canonical(source_project_root() / dir);
      } catch (const std::exception&) {
        dir = fs::absolute(source_project_root() / dir);
      }
    }
    if (!opts.quiet) {
      explain_calibration_requirements(info, params, dir);
    }
    std::array<char, MAX_PATHNAME_LENGTH> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", dir.string().c_str());
    const auto err = SAMPIC256CH_LoadAllCalibValuesFromFiles(&info, &params, buffer.data());
    if (err != SAMPIC256CH_Success) {
      std::cerr << "Warning: calibration load failed (code " << static_cast<int>(err)
                << ")\n";
    }
  }

  const int board_index = (opts.board_index >= 0) ? opts.board_index : 0;
  if (board_index >= info.NbOfFeBoards) {
    std::cerr << "Board index " << board_index << " out of range ("
              << info.NbOfFeBoards << " detected)\n";
    return 1;
  }
  configure_self_trigger(info, params, opts, board_index);

  EventStruct event{};
  SAMPIC256CH_ErrCode err = SAMPIC256CH_StartRun(&info, &params, TRUE);
  if (err != SAMPIC256CH_Success) {
    std::cerr << "Failed to start run (code " << static_cast<int>(err) << ")\n";
    return 1;
  }

  auto stop_run = [&]() {
    SAMPIC256CH_StopRun(&info, &params);
  };

  bool captured = false;
  for (int evt = 0; evt < opts.max_events && !(stop_flag_ && *stop_flag_); ++evt) {
    SAMPIC256CH_PrepareEvent(&info, &params);
    err = SAMPIC256CH_NoFrameRead;
    int nframes = 0;
    int hits = 0;
    int loop_counter = 0;

    while (err != SAMPIC256CH_Success) {
      err = SAMPIC256CH_ReadEventBuffer(&info, 0, event_buffer, ml_frames, &nframes);
      if (err == SAMPIC256CH_Success) {
        err = SAMPIC256CH_DecodeEvent(&info, &params, ml_frames, &event, nframes, &hits);
      }
      if (err == SAMPIC256CH_AcquisitionError || err == SAMPIC256CH_ErrInvalidEvent) {
        stop_run();
        throw std::runtime_error("Acquisition error while reading calibration data");
      }
      if ((loop_counter % opts.prepare_interval) == 0) {
        SAMPIC256CH_PrepareEvent(&info, &params);
      }
      if (loop_counter >= opts.max_loops) {
        stop_run();
        throw std::runtime_error("Exceeded read loop retry budget");
      }
      ++loop_counter;
      if (err != SAMPIC256CH_Success) {
        std::this_thread::sleep_for(std::chrono::microseconds(opts.retry_sleep_us));
      }
    }

    if (hits > 0) {
      std::cout << "Event captured with " << hits << " hit(s)\n";
      print_hit(event.Hit[0], opts.samples_to_print);
      captured = true;
      break;
    }
  }

  stop_run();
  if (!captured) {
    std::cout << "No hits captured within " << opts.max_events << " events.\n";
    return 1;
  }
  return 0;
}

CalibrationCheckOptions CalibrationMode::parse_args(int argc, char** argv) {
  CalibrationCheckOptions opts;
  for (int i = 0; i < argc; ++i) {
    std::string_view arg{argv[i]};
    auto require_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + std::string(name));
      }
      return std::string(argv[++i]);
    };

    if (arg == "--ip") {
      opts.ip = require_value(arg);
    } else if (arg == "--port") {
      opts.port = std::stoi(require_value(arg));
    } else if (arg == "--board") {
      opts.board_index = std::stoi(require_value(arg));
    } else if (arg == "--threshold") {
      opts.threshold = std::stod(require_value(arg));
    } else if (arg == "--max-events") {
      opts.max_events = std::stoi(require_value(arg));
    } else if (arg == "--samples") {
      opts.samples_to_print = std::stoi(require_value(arg));
    } else if (arg == "--prepare-interval") {
      opts.prepare_interval = std::stoi(require_value(arg));
    } else if (arg == "--max-loops") {
      opts.max_loops = std::stoi(require_value(arg));
    } else if (arg == "--retry-us") {
      opts.retry_sleep_us = std::stoi(require_value(arg));
    } else if (arg == "--calibration-dir") {
      opts.calibration_dir = require_value(arg);
    } else if (arg == "--no-calibration") {
      opts.load_calibration = false;
    } else if (arg == "--quiet") {
      opts.quiet = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Calibration mode options:\n"
                << "  --ip <addr>              Crate IP (default 192.168.0.4)\n"
                << "  --port <port>            Crate port (default 27015)\n"
                << "  --board <index>          Front-end board index (-1 = first detected)\n"
                << "  --threshold <volts>      Self-trigger threshold (default 0.1 V)\n"
                << "  --max-events <n>         Max events to search for a hit (default 50)\n"
                << "  --samples <n>            Number of samples to print (default 16)\n"
                << "  --prepare-interval <n>   Re-send prepare after N loops\n"
                << "  --max-loops <n>          Retry limit while reading frames\n"
                << "  --retry-us <µs>          Sleep between retries (default 100)\n"
                << "  --calibration-dir <dir>  Calibration directory (default resources/calib)\n"
                << "  --no-calibration         Skip loading calibration files\n"
                << "  --quiet                  Suppress ancillary output\n";
      std::exit(0);
    } else {
      throw std::runtime_error("Unknown calibration option: " + std::string(arg));
    }
  }
  if (opts.max_events <= 0) {
    throw std::runtime_error("--max-events must be positive");
  }
  if (opts.samples_to_print <= 0) {
    throw std::runtime_error("--samples must be positive");
  }
  return opts;
}

}  // namespace sampic::calibration
