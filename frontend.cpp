// ======================================================================
// SAMPIC Frontend (buffer-based, ODB-driven; controller owns collector)
// ======================================================================
// System
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <unistd.h>

// MIDAS
#include "midas.h"
#include "mfe.h"

// Project: ODB + logging + FE config
#include "integration/midas/frontend_config.h"
#include "integration/midas/odb/odb_manager.h"
#include "integration/midas/odb/odb_utils.h"
#include "integration/spdlog/logger_config.h"
#include "integration/spdlog/logger_configurator.h"

// Project: SAMPIC configs + controller
#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/controller/sampic_controller.h"

// Event + timing structs
#include "integration/sampic/collector/sampic_event_buffer.h"

// ======================================================================
// Globals (MIDAS)
// ======================================================================
const char *frontend_name       = "SAMPIC_Frontend";
const char *frontend_file_name  = __FILE__;
BOOL        frontend_call_loop  = FALSE;
INT         display_period      = 0;
INT         max_event_size      = 128 * 1024 * 1024;
INT         max_event_size_frag = 5 * max_event_size;
INT         event_buffer_size   = 5 * max_event_size;

static INT  g_frontend_index     = 0;
static char g_settings_path[256] = {0};

static bool g_system_initialized = false;

// Polling
static std::chrono::steady_clock::time_point g_last_poll_time;
static std::chrono::microseconds             g_polling_interval(1'000'000); // default 1s
static std::chrono::steady_clock::time_point g_last_evt_ts = std::chrono::steady_clock::time_point::min();

// Configs read from ODB
static FrontendConfig         g_fe_cfg;
static LoggerConfig           g_logger_cfg;
static SampicSystemSettings   g_sys_cfg;
static SampicControllerConfig g_ctrl_cfg;
static SampicCollectorConfig  g_coll_cfg;

// Controller (owns collector + buffer)
static std::unique_ptr<SampicController> g_controller;

// ======================================================================
// Prototypes
// ======================================================================
INT frontend_init(void);
INT frontend_exit(void);
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop(void);

INT read_sampic_event(char *pevent, INT off);
INT poll_event(INT source, INT count, BOOL test);
INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

// ======================================================================
// Equipment
// ======================================================================
BOOL equipment_common_overwrite = TRUE;

EQUIPMENT equipment[] = {
    {"SAMPIC %02d",
        { 1, 0,
          "SYSTEM",
          EQ_POLLED | EQ_EB,
          0,
          "MIDAS",
          TRUE,
          RO_RUNNING,
          100,
          0,
          0,
          TRUE,
          "", "", "", },
        read_sampic_event
    },
    {""}
};

// ======================================================================
// Utilities
// ======================================================================
static std::string make_bank_name(const std::string& prefix, int idx2d) {
  std::string p = prefix.empty() ? "XX" : prefix.substr(0,2);
  char name[8];
  std::snprintf(name, sizeof(name), "%s%02d", p.c_str(), idx2d);
  return std::string(name);
}

// ======================================================================
// ODB helpers
// ======================================================================
static bool initialize_all_configs_from_odb(std::string& err_out) {
  try {
    OdbManager odb;
    const std::string base = g_settings_path;

    odb.initialize(base + "/Logger", LoggerConfig{});
    g_logger_cfg = odb.read<LoggerConfig>(base + "/Logger");
    LoggerConfigurator::configure(g_logger_cfg);

    odb.initialize(base + "/Frontend", FrontendConfig{});
    g_fe_cfg = odb.read<FrontendConfig>(base + "/Frontend");

    odb.initialize(base + "/Crate", SampicSystemSettings{});
    g_sys_cfg = odb.read<SampicSystemSettings>(base + "/Crate");

    odb.initialize(base + "/Sampic Controller", SampicControllerConfig{});
    g_ctrl_cfg = odb.read<SampicControllerConfig>(base + "/Sampic Controller");

    odb.initialize(base + "/Sampic Event Collector", SampicCollectorConfig{});
    g_coll_cfg = odb.read<SampicCollectorConfig>(base + "/Sampic Event Collector");

    g_polling_interval = std::chrono::microseconds(g_fe_cfg.polling_interval_us);
    return true;
  } catch (const std::exception& e) {
    err_out = e.what();
    return false;
  }
}

static bool read_all_configs_from_odb(std::string& err_out) {
  try {
    OdbManager odb;
    const std::string base = g_settings_path;

    g_logger_cfg = odb.read<LoggerConfig>(base + "/Logger");
    g_fe_cfg     = odb.read<FrontendConfig>(base + "/Frontend");
    g_sys_cfg    = odb.read<SampicSystemSettings>(base + "/Crate");
    g_ctrl_cfg   = odb.read<SampicControllerConfig>(base + "/Sampic Controller");
    g_coll_cfg   = odb.read<SampicCollectorConfig>(base + "/Sampic Event Collector");

    LoggerConfigurator::configure(g_logger_cfg);
    g_polling_interval = std::chrono::microseconds(g_fe_cfg.polling_interval_us);
    return true;
  } catch (const std::exception& e) {
    err_out = e.what();
    return false;
  }
}

// ======================================================================
// SAMPIC controller setup
// ======================================================================
static bool initialize_sampic_controller(std::string& err_out) {
  try {
    g_controller = std::make_unique<SampicController>(g_sys_cfg, g_ctrl_cfg, g_coll_cfg);

    int rc = g_controller->initialize();
    if (rc != 0) {
      err_out = "SAMPIC controller initialize() failed with code " + std::to_string(rc);
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    err_out = e.what();
    return false;
  }
}

// ======================================================================
// MIDAS Frontend impl
// ======================================================================
INT frontend_init() {
  g_frontend_index = get_frontend_index();
  std::snprintf(g_settings_path, sizeof(g_settings_path),
                "/Equipment/SAMPIC %02d/Settings", g_frontend_index);

  OdbUtils::odbSetStatusColor(g_frontend_index, g_fe_cfg.init_color);

  std::string err;
  if (!initialize_all_configs_from_odb(err)) {
    cm_msg(MERROR, __FUNCTION__, "Failed to initialize configs: %s", err.c_str());
    return FE_ERR_ODB;
  }
  if (!initialize_sampic_controller(err)) {
    cm_msg(MERROR, __FUNCTION__, "Failed to init controller: %s", err.c_str());
    return FE_ERR_HW;
  }

  g_system_initialized = true;
  OdbUtils::odbSetStatusColor(g_frontend_index, g_fe_cfg.ready_color);
  return SUCCESS;
}

INT frontend_exit() {
  try {
    if (g_controller) {
      g_controller->stopCollector();
      g_controller->stopRun();
      g_controller->cleanup();
    }
  } catch (...) {}
  g_controller.reset();
  g_system_initialized = false;
  return SUCCESS;
}

INT begin_of_run(INT run_number, char *error) {
  if (!g_system_initialized || !g_controller) {
    std::strcpy(error, "System not initialized");
    return FE_ERR_HW;
  }

  std::string err;
  if (!read_all_configs_from_odb(err)) {
    std::snprintf(error, 256, "Failed to refresh configs: %s", err.c_str());
    return FE_ERR_ODB;
  }

  g_controller->setSystemSettings(g_sys_cfg);
  g_controller->setControllerConfig(g_ctrl_cfg);
  g_controller->setCollectorConfig(g_coll_cfg);

  if (g_controller->applySettings() != 0) {
    std::strcpy(error, "Failed to apply settings");
    return FE_ERR_HW;
  }

  g_controller->startCollector();
  if (g_controller->startRun() != 0) {
    std::strcpy(error, "Failed to start run");
    return FE_ERR_HW;
  }

  g_last_evt_ts = std::chrono::steady_clock::time_point::min();
  return SUCCESS;
}

INT end_of_run(INT, char *error) {
  try {
    if (g_controller) {
      g_controller->stopCollector();
      if (g_controller->stopRun() != 0) {
        std::strcpy(error, "Failed to stop run");
        return FE_ERR_HW;
      }
    }
  } catch (...) {
    std::strcpy(error, "Error during EOR");
    return FE_ERR_HW;
  }
  return SUCCESS;
}

INT pause_run(INT, char*)  { return SUCCESS; }
INT resume_run(INT, char*) { return SUCCESS; }
INT frontend_loop()        { return SUCCESS; }

// ======================================================================
// Polling: ask buffer if there's anything newer
// ======================================================================
INT poll_event(INT, INT, BOOL test) {
  if (!g_system_initialized || !g_controller) return test ? FALSE : 0;

  auto now = std::chrono::steady_clock::now();
  if (now - g_last_poll_time < g_polling_interval) return test ? FALSE : 0;
  g_last_poll_time = now;

  if (g_controller->buffer().hasNewSince(g_last_evt_ts)) return TRUE;
  return test ? FALSE : 0;
}

INT interrupt_configure(INT, INT, POINTER_T) { return SUCCESS; }

// ======================================================================
// Readout: dump all new EventStructs + timing into banks
// ======================================================================
INT read_sampic_event(char *pevent, INT) {
  if (!g_system_initialized || !g_controller) return 0;

  auto new_events = g_controller->buffer().getSince(g_last_evt_ts);

  if (new_events.empty()) {
    // We were polled because hasNewSince() returned true, but got no events.
    // Emit a hard error with useful context.
    const auto latest_opt = g_controller->buffer().latest();
    const auto ns_since_epoch = [](std::chrono::steady_clock::time_point tp) -> long long {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
    };

    const long long last_pub_ns = ns_since_epoch(g_last_evt_ts);
    const long long latest_ns   = latest_opt
      ? ns_since_epoch(latest_opt->timestamp)
      : -1LL;

    cm_msg(MERROR, "SAMPIC",
           "Poll indicated new data but getSince() returned none. "
           "last_published_ts_ns=%lld, buffer_size=%zu, latest_ts_ns=%lld",
           last_pub_ns,
           g_controller->buffer().size(),
           latest_ns);

    // Optional: advance to the current latest to avoid repeated errors if this was a race.
    if (latest_opt) g_last_evt_ts = latest_opt->timestamp;

    return 0;
  }

  bk_init32(pevent);

  // --- Data bank ---
  auto data_bank = make_bank_name(g_fe_cfg.data_bank_prefix, g_frontend_index);
  uint8_t* pdata = nullptr;
  bk_create(pevent, data_bank.c_str(), TID_UINT8, (void**)&pdata);

  // --- Timing bank ---
  auto timing_bank = make_bank_name(g_fe_cfg.timing_bank_prefix, g_frontend_index);
  uint8_t* ptiming = nullptr;
  bk_create(pevent, timing_bank.c_str(), TID_UINT8, (void**)&ptiming);

  struct TimingPayload {
    uint64_t timestamp_ns;
    uint32_t prepare_us, read_us, decode_us, total_us;
  };

  for (const auto& tse : new_events) {
    std::memcpy(pdata, &tse.event, sizeof(EventStruct));
    pdata += sizeof(EventStruct);

    TimingPayload tp;
    tp.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        tse.timestamp.time_since_epoch()).count();
    tp.prepare_us = tse.timing.prepare_duration.count();
    tp.read_us    = tse.timing.read_duration.count();
    tp.decode_us  = tse.timing.decode_duration.count();
    tp.total_us   = tse.timing.total_duration.count();

    std::memcpy(ptiming, &tp, sizeof(tp));
    ptiming += sizeof(tp);
  }

  // Update last timestamp to the newest event we just packed
  g_last_evt_ts = new_events.back().timestamp;

  bk_close(pevent, pdata);
  bk_close(pevent, ptiming);

  return bk_size(pevent);
}


