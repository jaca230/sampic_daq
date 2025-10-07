// ======================================================================
// SAMPIC Frontend (ODB-driven; uses SampicController + FrontendEventCollector)
// ======================================================================

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

// Project: SAMPIC controller + configs
#include "integration/sampic/config/sampic_crate_config.h"
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/controller/sampic_controller.h"

// Project: Frontend collector (higher-level grouping)
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"
#include "processing/sampic_processing/collector/banks/frontend_event_bank_data.h"

// ======================================================================
// Globals
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

// Polling / timing
static std::chrono::steady_clock::time_point g_last_poll_time;
static std::chrono::microseconds             g_polling_interval(1'000'000);
static std::chrono::steady_clock::time_point g_last_evt_ts =
    std::chrono::steady_clock::time_point::min();

// ODB-driven configs
static FrontendConfig              g_fe_cfg;
static LoggerConfig                g_logger_cfg;
static SampicSystemSettings        g_sys_cfg;
static SampicControllerConfig      g_ctrl_cfg;
static SampicCollectorConfig       g_coll_cfg;
static FrontendEventCollectorConfig g_fe_coll_cfg;

// Core objects
static std::unique_ptr<SampicController>       g_controller;
static std::unique_ptr<FrontendEventCollector> g_frontend_collector;

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
// Equipment definition
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
// Utility
// ======================================================================
static std::string make_bank_name(const std::string& prefix, int idx2d) {
    std::string p = prefix.empty() ? "XX" : prefix.substr(0, 2);
    char name[8];
    std::snprintf(name, sizeof(name), "%s%02d", p.c_str(), idx2d);
    return std::string(name);
}

// ======================================================================
// ODB configuration
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

        odb.initialize(base + "/Frontend Event Collector", FrontendEventCollectorConfig{});
        g_fe_coll_cfg = odb.read<FrontendEventCollectorConfig>(base + "/Frontend Event Collector");

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
        g_fe_coll_cfg = odb.read<FrontendEventCollectorConfig>(base + "/Frontend Event Collector");

        LoggerConfigurator::configure(g_logger_cfg);
        g_polling_interval = std::chrono::microseconds(g_fe_cfg.polling_interval_us);
        return true;
    } catch (const std::exception& e) {
        err_out = e.what();
        return false;
    }
}

// ======================================================================
// SAMPIC controller init
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
// MIDAS lifecycle
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

    // Create frontend collector using controller's buffer
    g_frontend_collector = std::make_unique<FrontendEventCollector>(
        g_controller->buffer(),  // direct buffer reference
        g_fe_coll_cfg
    );

    spdlog::info("FrontendEventCollector created (mode={}, buffer_size={})",
                 static_cast<int>(g_fe_coll_cfg.mode), g_fe_coll_cfg.buffer_size);

    g_system_initialized = true;
    OdbUtils::odbSetStatusColor(g_frontend_index, g_fe_cfg.ready_color);
    return SUCCESS;
}

INT begin_of_run(INT, char *error) {
    try {
        if (!g_system_initialized || !g_controller) {
            std::strcpy(error, "System not initialized");
            return FE_ERR_HW;
        }

        std::string err;
        if (!read_all_configs_from_odb(err)) {
            std::snprintf(error, 256, "Failed to refresh configs: %s", err.c_str());
            return FE_ERR_ODB;
        }

        // --- Apply SAMPIC controller configs
        g_controller->setSystemSettings(g_sys_cfg);
        g_controller->setControllerConfig(g_ctrl_cfg);
        g_controller->setCollectorConfig(g_coll_cfg);

        if (g_controller->applySettings() != 0) {
            std::strcpy(error, "Failed to apply SAMPIC settings");
            return FE_ERR_HW;
        }

        // --- Apply FrontendEventCollector configs (if available)
        if (g_frontend_collector) {
            g_frontend_collector->setConfig(g_fe_coll_cfg);
            if (g_frontend_collector->applySettings() != 0) {
                std::strcpy(error, "Failed to apply frontend collector settings");
                return FE_ERR_HW;
            }
        } else {
            spdlog::warn("FrontendEventCollector missing during begin_of_run()");
        }

        // --- Start everything
        g_controller->startCollector();
        if (g_controller->startRun() != 0) {
            std::strcpy(error, "Failed to start SAMPIC run");
            return FE_ERR_HW;
        }

        if (g_frontend_collector)
            g_frontend_collector->start();

        spdlog::info("FrontendEventCollector started.");
        g_last_evt_ts = std::chrono::steady_clock::time_point::min();
        return SUCCESS;

    } catch (const std::exception& e) {
        std::snprintf(error, 256, "Exception in begin_of_run: %s", e.what());
        spdlog::error("begin_of_run() exception: {}", e.what());
        return FE_ERR_HW;
    } catch (...) {
        std::strcpy(error, "Unknown exception in begin_of_run");
        spdlog::error("begin_of_run() unknown exception");
        return FE_ERR_HW;
    }
}



INT end_of_run(INT, char *error) {
    try {
        if (g_frontend_collector)
            g_frontend_collector->stop();
        if (g_controller) {
            g_controller->stopCollector();
            g_controller->stopRun();
        }
    } catch (const std::exception& e) {
        std::snprintf(error, 256, "Error during EOR: %s", e.what());
        return FE_ERR_HW;
    }
    return SUCCESS;
}


INT pause_run(INT, char*)  { return SUCCESS; }
INT resume_run(INT, char*) { return SUCCESS; }
INT frontend_loop()        { return SUCCESS; }

INT frontend_exit() {
    try {
        if (g_frontend_collector) g_frontend_collector->stop();
        if (g_controller) {
            g_controller->stopCollector();
            g_controller->stopRun();
            g_controller->cleanup();
        }
    } catch (...) {}
    g_frontend_collector.reset();
    g_controller.reset();
    g_system_initialized = false;
    return SUCCESS;
}

// ======================================================================
// Polling
// ======================================================================
INT poll_event(INT, INT, BOOL test) {
    if (!g_system_initialized || !g_frontend_collector)
        return test ? FALSE : 0;

    auto now = std::chrono::steady_clock::now();
    if (now - g_last_poll_time < g_polling_interval)
        return test ? FALSE : 0;

    g_last_poll_time = now;
    if (g_frontend_collector->buffer().hasNewSince(g_last_evt_ts))
        return TRUE;

    return test ? FALSE : 0;
}

INT interrupt_configure(INT, INT, POINTER_T) { return SUCCESS; }

// ======================================================================
// Readout (FrontendEvent → multiple banks)
// ======================================================================
INT read_sampic_event(char *pevent, INT)
{
    if (!g_system_initialized || !g_frontend_collector)
        return 0;

    const auto t_start = std::chrono::steady_clock::now();

    auto& fbuf = g_frontend_collector->buffer();
    const auto new_events = fbuf.getSince(g_last_evt_ts);
    if (new_events.empty())
        return 0;

    bk_init32(pevent);

    for (size_t i = 0; i < new_events.size(); ++i) {
        const auto t_evt_start = std::chrono::steady_clock::now();
        const auto& fev = new_events[i];

        for (const auto& bank : fev->banks()) {
            const std::string bank_name = make_bank_name(bank->bankPrefix(), g_frontend_index);
            uint8_t* pdata = nullptr;
            bk_create(pevent, bank_name.c_str(), TID_UINT8, (void**)&pdata);
            uint8_t* const pstart = pdata;

            if (const auto* multi = dynamic_cast<const FrontendEventBankData*>(bank.get())) {
                for (const auto& [ptr, len] : multi->slices()) {
                    std::memcpy(pdata, ptr, len);
                    pdata += len;
                }
            } else {
                const uint8_t* src = bank->data();
                const size_t len = bank->size();
                if (src && len > 0) {
                    std::memcpy(pdata, src, len);
                    pdata += len;
                }
            }

            bk_close(pevent, pdata);
            spdlog::trace("FrontendEvent[{}] → wrote bank {} ({} bytes)",
                          i, bank_name, static_cast<int>(pdata - pstart));
        }

        const auto t_evt_end = std::chrono::steady_clock::now();
        const auto dur_evt_us =
            std::chrono::duration_cast<std::chrono::microseconds>(t_evt_end - t_evt_start).count();
        spdlog::trace("FrontendEvent[{}] serialization took {} µs", i, dur_evt_us);
    }

    g_last_evt_ts = new_events.back()->timestamp();

    const int total_size = bk_size(pevent);
    const auto t_end = std::chrono::steady_clock::now();
    const auto dur_total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();

    spdlog::debug("read_sampic_event: wrote {} FrontendEvents, total MIDAS size={} B ({} µs)",
                  new_events.size(), total_size, dur_total_us);

    return total_size;
}
