#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sampic::lecroy {

struct LecroyChannelConfig {
  std::string channel = "A";
  double amplitude_v = 1.0;
  double baseline_v = 0.0;
  double width_ns = 2.4;
  double lead_ns = 1.8;
  double trail_ns = 1.9;
  bool output_main = true;
  bool output_inverse = false;
  bool double_pulse_enabled = true;
};

struct LecroyConfig {
  std::string ip = "10.0.1.102";
  int port = 1234;
  double frequency_hz = 80e6;
  std::string trigger_mode = "NORMAL";
  std::string trigger_slope = "POS";
  double trigger_level_volts = 1.5;
  LecroyChannelConfig channel;
  double initial_delay_ns = 40.0;
  bool manual_trigger = false;
  double manual_trigger_interval_s = 0.01;
  double settle_delay_s = 0.2;
};

class LecroyClient {
 public:
  LecroyClient();
  ~LecroyClient();

  void Configure(const LecroyConfig& config);
  void SetDoublePulseDelay(double delay_ns);
  void SetFrequency(double frequency_hz);
  void Trigger();
  std::string Query(const std::string& command);

 private:
  void EnsureConnected();
  void Disconnect();
  void SendCommand(const std::string& command);
  void ApplyChannelConfig();

  LecroyConfig config_{};
  int socket_fd_ = -1;
};

}  // namespace sampic::lecroy
