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
#include "sampic_tests/modes/double_pulse/config.h"

namespace sampic::double_pulse {

struct AppliedSettings {
  int sampling_frequency_mhz = 0;
  bool use_external_clock = false;
  std::vector<int> enabled_channels;
};

class SampicSession {
 public:
  SampicSession(const ConnectionConfig& conn,
                const ExternalTriggerConfig& trigger_cfg);
  ~SampicSession();

  SampicSession(const SampicSession&) = delete;
  SampicSession& operator=(const SampicSession&) = delete;

  void configure_for_combo(const ParameterCombination& combo,
                           int board_index,
                           const std::vector<int>& channels);
  void set_threshold(int board_index, double threshold_volts);

  AppliedSettings readback_settings(int board_index);

  bool start_run_with_retry(const StartRetryConfig& retry_cfg,
                            int& attempts_out,
                            std::vector<std::string>& errors);

  void stop_run();

  scan::SampleStats acquire_sample(const ReadoutConfig& readout_cfg,
                                   double duration_s,
                                   volatile std::sig_atomic_t* stop_flag,
                                   bool capture_hits = false,
                                   std::vector<scan::HitRecord>* hits_out = nullptr);

 private:
  void initialise_connection();
  void configure_base();
  void allocate_event_memory();
  void configure_channel_defaults();
  void configure_sampling(int rate_mhz);
  void configure_channel_mask(int board_index, const std::vector<int>& channels);
  void check(SAMPIC256CH_ErrCode err, std::string_view what);

  ConnectionConfig conn_opts_;
  ExternalTriggerConfig trigger_opts_;
  CrateConnectionParamStruct conn_{};
  CrateInfoStruct info_{};
  CrateParamStruct params_{};
  void* event_buffer_ = nullptr;
  ML_Frame* ml_frames_ = nullptr;
  bool run_active_ = false;
  bool connected_ = false;
};

}  // namespace sampic::double_pulse
