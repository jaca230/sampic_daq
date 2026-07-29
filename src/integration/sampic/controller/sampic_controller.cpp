#include "integration/sampic/controller/sampic_controller.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_default.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_example.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_simulator.h"
#include "integration/sampic/controller/init_settings_modes/sampic_init_settings_mode_simulator_parport_trigger.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_default.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_example.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_simulator.h"
#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_simulator_parport_trigger.h"

#include <parport_trigger/trigger_client.hpp>
#include <parport_trigger/trigger_server.hpp>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <algorithm>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

bool can_connect_to_unix_socket(const std::string& socket_path) {
    if (socket_path.empty() || socket_path.size() >= sizeof(sockaddr_un::sun_path)) {
        return false;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    const int rc = ::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return rc == 0;
}

} // namespace

SampicController::SampicController(const SampicSystemSettings& sys_cfg,
                                   const SampicControllerConfig& ctrl_cfg,
                                   const SampicCollectorConfig& coll_cfg)
    : settings_(sys_cfg),
      ctrl_cfg_(ctrl_cfg),
      coll_cfg_(coll_cfg)
{
    // Select init mode
    switch (ctrl_cfg_.init_mode) {
        case SampicInitSettingsModeType::DEFAULT:
            init_mode_ = std::make_unique<SampicInitSettingsModeDefault>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
        case SampicInitSettingsModeType::EXAMPLE:
            init_mode_ = std::make_unique<SampicInitSettingsModeExample>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
        case SampicInitSettingsModeType::SIMULATOR:
            init_mode_ = std::make_unique<SampicInitSettingsModeSimulator>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
        case SampicInitSettingsModeType::SIMULATOR_PP_TRIG:
            init_mode_ = std::make_unique<SampicInitSettingsModeSimulatorParportTrigger>(
                info_, params_, eventBuffer_, mlFrames_, settings_, ctrl_cfg_);
            break;
    }

    // Select apply mode
    switch (ctrl_cfg_.apply_mode) {
        case SampicApplySettingsModeType::DEFAULT:
            apply_mode_ = std::make_unique<SampicApplySettingsModeDefault>(
                info_, params_, settings_, ctrl_cfg_);
            break;
        case SampicApplySettingsModeType::EXAMPLE:
            apply_mode_ = std::make_unique<SampicApplySettingsModeExample>(
                info_, params_, settings_, ctrl_cfg_);
            break;
        case SampicApplySettingsModeType::SIMULATOR:
            apply_mode_ = std::make_unique<SampicApplySettingsModeSimulator>(
                info_, params_, settings_, ctrl_cfg_);
            break;
        case SampicApplySettingsModeType::SIMULATOR_PP_TRIG:
            apply_mode_ = std::make_unique<SampicApplySettingsModeSimulatorParportTrigger>(
                info_, params_, settings_, ctrl_cfg_);
            break;
    }

    // Create collector (owns its buffer)
    collector_ = std::make_unique<SampicCollector>(
        coll_cfg_, info_, params_, eventBuffer_, mlFrames_, trigger_client_);
}

SampicController::~SampicController() {
    try {
        stopCollector();
        stopRun();
        cleanup();
    } catch (...) {
        // swallow errors in destructor
    }
}

// ---------------- Config management ----------------
void SampicController::setSystemSettings(const SampicSystemSettings& s) { settings_ = s; }
SampicSystemSettings& SampicController::systemSettings() { return settings_; }
const SampicSystemSettings& SampicController::systemSettings() const { return settings_; }

void SampicController::setControllerConfig(const SampicControllerConfig& c) { ctrl_cfg_ = c; }
SampicControllerConfig& SampicController::controllerConfig() { return ctrl_cfg_; }
const SampicControllerConfig& SampicController::controllerConfig() const { return ctrl_cfg_; }

void SampicController::setCollectorConfig(const SampicCollectorConfig& c) { coll_cfg_ = c; }
SampicCollectorConfig& SampicController::collectorConfig() { return coll_cfg_; }
const SampicCollectorConfig& SampicController::collectorConfig() const { return coll_cfg_; }

// ---------------- Lifecycle ----------------
int SampicController::initialize() {
    if (!init_mode_) {
        spdlog::error("Init mode not configured");
        return -1;
    }
    int rc = init_mode_->initialize();
    if (rc == SAMPIC256CH_Success &&
        ctrl_cfg_.init_mode == SampicInitSettingsModeType::SIMULATOR_PP_TRIG) {
        rc = ensureParportTriggerInfrastructure();
        if (rc == 0) {
            collector_.reset();
            collector_ = std::make_unique<SampicCollector>(
                coll_cfg_, info_, params_, eventBuffer_, mlFrames_, trigger_client_);
        }
    }
    initialized_ = (rc == SAMPIC256CH_Success);
    return rc;
}

int SampicController::applySettings() {
    if (!apply_mode_) {
        spdlog::error("Apply mode not configured");
        return -1;
    }
    try {
        // Apply hardware settings
        apply_mode_->apply();

        if (ctrl_cfg_.init_mode == SampicInitSettingsModeType::SIMULATOR_PP_TRIG) {
            const int rc = ensureParportTriggerInfrastructure();
            if (rc != 0) {
                return rc;
            }
        } else {
            stopParportTriggerInfrastructure();
        }

        // Rebuild collector with updated config
        stopCollector();
        collector_.reset();
        collector_ = std::make_unique<SampicCollector>(
            coll_cfg_, info_, params_, eventBuffer_, mlFrames_, trigger_client_);

        spdlog::info("Collector rebuilt with new configuration");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Apply settings failed: {}", e.what());
        return -1;
    }
}

int SampicController::startRun() {
    if (run_started_) {
        spdlog::warn("startRun() called but run already started");
        return 0;
    }

    spdlog::info("Starting SAMPIC run...");
    if (isSoftwareOnlyInitMode()) {
        run_started_ = true;
        return 0;
    }

    auto err = SAMPIC256CH_StartRun(&info_, &params_, TRUE);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to start run (err={})", static_cast<int>(err));
        return err;
    }
    run_started_ = true;
    return 0;
}

int SampicController::stopRun() {
    if (!run_started_) {
        spdlog::debug("stopRun() called but run was not started — skipping");
        return 0;
    }

    spdlog::info("Stopping SAMPIC run...");
    if (isSoftwareOnlyInitMode()) {
        run_started_ = false;
        return 0;
    }

    auto err = SAMPIC256CH_StopRun(&info_, &params_);
    if (err != SAMPIC256CH_Success) {
        spdlog::error("Failed to stop run (err={})", static_cast<int>(err));
        return err;
    }
    run_started_ = false;
    return 0;
}

void SampicController::cleanup() {
    if (!initialized_) {
        stopParportTriggerInfrastructure();
        spdlog::debug("cleanup() called but controller not initialized — skipping");
        return;
    }

    spdlog::info("Cleaning up SAMPIC resources...");
    if (!isSoftwareOnlyInitMode()) {
        if (eventBuffer_ || mlFrames_) {
            SAMPIC256CH_FreeEventMemory(&eventBuffer_, &mlFrames_);
            eventBuffer_ = nullptr;
            mlFrames_ = nullptr;
        }
        SAMPIC256CH_CloseCrateConnection(&info_);
    }
    stopParportTriggerInfrastructure();
    initialized_ = false;
}

bool SampicController::isSoftwareOnlyInitMode() const {
    return ctrl_cfg_.init_mode == SampicInitSettingsModeType::SIMULATOR ||
           ctrl_cfg_.init_mode == SampicInitSettingsModeType::SIMULATOR_PP_TRIG;
}

int SampicController::ensureParportTriggerInfrastructure() {
    const auto& mode_cfg = coll_cfg_.simulator_pp_trig_mode;

    if (trigger_server_) {
        trigger_server_->stop();
        trigger_server_.reset();
    }
    if (trigger_client_) {
        trigger_client_->stop();
        trigger_client_.reset();
    }

    const bool server_alive = can_connect_to_unix_socket(mode_cfg.socket_path);
    if (!server_alive && mode_cfg.auto_start_server) {
        parport_trigger::TriggerServerConfig server_cfg{};
        server_cfg.device_path = mode_cfg.device_path;
        server_cfg.socket_path = mode_cfg.socket_path;
        server_cfg.poll_timeout = std::chrono::milliseconds(
            static_cast<int>(std::max<std::uint32_t>(1, mode_cfg.server_poll_timeout_ms)));

        trigger_server_ = std::make_unique<parport_trigger::TriggerServer>(server_cfg);
        trigger_server_->start();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!trigger_server_->is_running()) {
            const auto err = trigger_server_->last_error();
            spdlog::error("Failed to start parport trigger server: {}", err.empty() ? "unknown error" : err);
            trigger_server_.reset();
            return -1;
        }

        spdlog::info("Started in-process parport trigger server at unix://{} using {}",
                     mode_cfg.socket_path, mode_cfg.device_path);
    } else if (!server_alive) {
        spdlog::warn("No parport trigger server is listening on unix://{} and auto_start_server=false",
                     mode_cfg.socket_path);
    }

    parport_trigger::TriggerClientConfig client_cfg{};
    client_cfg.socket_path = mode_cfg.socket_path;
    client_cfg.auto_start_server = false;
    client_cfg.server_device_path = mode_cfg.device_path;
    client_cfg.connect_timeout = std::chrono::milliseconds(
        static_cast<int>(std::max<std::uint32_t>(1, mode_cfg.connect_timeout_ms)));
    client_cfg.retry_interval = std::chrono::milliseconds(
        static_cast<int>(std::max<std::uint32_t>(1, mode_cfg.retry_interval_ms)));
    client_cfg.queue_capacity = std::max<std::size_t>(
        1, static_cast<std::size_t>(mode_cfg.queue_capacity));

    trigger_client_ = std::make_shared<parport_trigger::TriggerClient>(std::move(client_cfg));
    trigger_client_->start();

    if (!trigger_client_->is_running()) {
        spdlog::error("Failed to start parport trigger client");
        trigger_client_.reset();
        return -1;
    }

    spdlog::info("Parport trigger client started (socket='{}')", mode_cfg.socket_path);
    return 0;
}

void SampicController::stopParportTriggerInfrastructure() {
    if (trigger_server_) {
        trigger_server_->stop();
        trigger_server_.reset();
    }
    if (trigger_client_) {
        trigger_client_->stop();
        trigger_client_.reset();
    }
}

// ---------------- Collector ----------------
void SampicController::startCollector() {
    if (collector_ && !collector_running_) {
        collector_->start();
        collector_running_ = true;
    }
}
void SampicController::stopCollector() {
    if (collector_ && collector_running_) {
        collector_->stop();
        collector_running_ = false;
    }
}

// ---------------- Buffer access ----------------
SampicEventBuffer& SampicController::buffer() {
    return collector_->buffer();
}
const SampicEventBuffer& SampicController::buffer() const {
    return collector_->buffer();
}
