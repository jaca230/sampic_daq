#pragma once

#include <csignal>
#include <memory>
#include <optional>
#include <vector>

extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#include "sampic_tests/common/scan_types.h"
#include "sampic_tests/modes/deadtime/config.h"

namespace sampic::deadtime {

struct AppliedSettings {
  int sampling_frequency_mhz = 0;
  bool use_external_clock = false;
  int pulser_period_ticks = 0;
  std::vector<int> enabled_channels;
};

class SampicSession {
 public:
  explicit SampicSession(const ConnectionConfig& conn);
  ~SampicSession();

  SampicSession(const SampicSession&) = delete;
  SampicSession& operator=(const SampicSession&) = delete;

  void configure_for_combo(const ParameterCombination& combo, int board_index);

  AppliedSettings readback_settings(int board_index);

  bool start_run_with_retry(const StartRetryConfig& retry_cfg,
                            int& attempts_out,
                            std::vector<std::string>& errors);

  void stop_run();

  scan::SampleStats acquire_sample(const ReadoutConfig& readout_cfg,
                                   double duration_s,
                                   volatile std::sig_atomic_t* stop_flag);

 private:
  void initialise_connection();
  void configure_base();
  void allocate_event_memory();
  void configure_channel_defaults();
  void configure_sampling(int rate_mhz);
  void configure_pulser(int pulser_ticks);
  void configure_channel_mask(int board_index, const std::vector<int>& channels);
  void check(SAMPIC256CH_ErrCode err, std::string_view what);

  ConnectionConfig conn_opts_;
  CrateConnectionParamStruct conn_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool run_active_ = false;
  bool connected_ = false;
};

}  // namespace sampic::deadtime
