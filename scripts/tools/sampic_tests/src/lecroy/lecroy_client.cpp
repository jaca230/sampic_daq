#include "sampic_tests/lecroy/lecroy_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace sampic::lecroy {

namespace {

std::string FormatScientific(double value) {
  std::ostringstream oss;
  oss << std::uppercase << std::scientific << std::setprecision(6) << value;
  return oss.str();
}

}  // namespace

LecroyClient::LecroyClient() = default;

LecroyClient::~LecroyClient() {
  Disconnect();
}

void LecroyClient::Configure(const LecroyConfig& cfg) {
  config_ = cfg;
  EnsureConnected();
  SendCommand("*CLS");
  SendCommand("TRMD " + config_.trigger_mode);
  SendCommand("TRSL " + config_.trigger_slope);
  SendCommand("TROV " + FormatScientific(config_.trigger_level_volts));
  SendCommand("FREQ " + FormatScientific(config_.frequency_hz));
  ApplyChannelConfig();
  SetDoublePulseDelay(config_.initial_delay_ns);
}

void LecroyClient::SetDoublePulseDelay(double delay_ns) {
  EnsureConnected();
  const double seconds = delay_ns * 1e-9;
  SendCommand(config_.channel.channel + ":DEL " + FormatScientific(seconds));
  if (config_.settle_delay_s > 0.0) {
    std::this_thread::sleep_for(std::chrono::duration<double>(config_.settle_delay_s));
  }
  if (config_.manual_trigger) {
    Trigger();
  }
}

void LecroyClient::Trigger() {
  EnsureConnected();
  SendCommand("*TRG");
}

std::string LecroyClient::Query(const std::string& command) {
  EnsureConnected();
  std::string payload = command;
  if (payload.empty() || payload.back() != '\n') payload.push_back('\n');
  if (::send(socket_fd_, payload.data(), payload.size(), 0) < 0) {
    throw std::runtime_error("Failed to send query: " + command);
  }
  char buffer[512];
  const auto received = ::recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);
  if (received <= 0) {
    throw std::runtime_error("Failed to read response for command: " + command);
  }
  buffer[received] = '\0';
  std::string result(buffer);
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
    result.pop_back();
  }
  return result;
}

void LecroyClient::EnsureConnected() {
  if (socket_fd_ >= 0) return;
  socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    throw std::runtime_error("Failed to create Lecroy socket");
  }
  timeval timeout{};
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  ::setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(config_.port));
  if (::inet_pton(AF_INET, config_.ip.c_str(), &addr.sin_addr) <= 0) {
    Disconnect();
    throw std::runtime_error("Invalid Lecroy IP: " + config_.ip);
  }
  if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    Disconnect();
    throw std::runtime_error("Failed to connect to Lecroy at " + config_.ip + ":" +
                             std::to_string(config_.port));
  }
}

void LecroyClient::Disconnect() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

void LecroyClient::SendCommand(const std::string& command) {
  EnsureConnected();
  std::string payload = command;
  if (payload.empty() || payload.back() != '\n') payload.push_back('\n');
  if (::send(socket_fd_, payload.data(), payload.size(), 0) < 0) {
    throw std::runtime_error("Failed to send command: " + command);
  }
}

void LecroyClient::ApplyChannelConfig() {
  const auto& ch = config_.channel;
  const std::string prefix = ch.channel + ":";
  SendCommand(prefix + "AMP " + FormatScientific(ch.amplitude_v));
  SendCommand(prefix + "BASE " + FormatScientific(ch.baseline_v));
  SendCommand(prefix + "WID " + FormatScientific(ch.width_ns * 1e-9));
  SendCommand(prefix + "LEAD " + FormatScientific(ch.lead_ns * 1e-9));
  SendCommand(prefix + "TRAIL " + FormatScientific(ch.trail_ns * 1e-9));
  SendCommand(prefix + "OUT " + std::string(ch.output_main ? "ON" : "OFF"));
  SendCommand(prefix + "OUTB " + std::string(ch.output_inverse ? "ON" : "OFF"));
  SendCommand(prefix + "DBL " + std::string(ch.double_pulse_enabled ? "ON" : "OFF"));
  SendCommand(prefix + "DISA OFF");
}

}  // namespace sampic::lecroy
