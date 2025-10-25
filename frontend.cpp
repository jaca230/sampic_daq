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
#include "integration/midas/frontend_odb_paths.h"
#include "integration/midas/frontend_runtime.h"
#include "integration/midas/odb/odb_utils.h"
#include <spdlog/spdlog.h>

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
// MIDAS lifecycle
// ======================================================================
INT frontend_init() {
    auto& runtime = frontend::runtime::Runtime::instance();
    runtime.frontendIndex = get_frontend_index();

    char settings_buffer[128];
    std::snprintf(settings_buffer, sizeof(settings_buffer),
                  frontend::odb::kSettingsBaseFormat, runtime.frontendIndex);
    runtime.settingsPath = settings_buffer;

    OdbUtils::odbSetStatusColor(runtime.frontendIndex, runtime.configs.frontend.init_color);

    std::string err;
    if (!runtime.loadInitialConfigs(err)) {
        cm_msg(MERROR, __FUNCTION__, "Failed to initialize configs: %s", err.c_str());
        return FE_ERR_ODB;
    }
    if (!runtime.initializeController(err)) {
        cm_msg(MERROR, __FUNCTION__, "Failed to init controller: %s", err.c_str());
        return FE_ERR_HW;
    }

    // Create frontend collector using controller's buffer
    runtime.collector = std::make_unique<FrontendEventCollector>(
        runtime.controller->buffer(),  // direct buffer reference
        runtime.configs.frontend_collector
    );

    spdlog::info("FrontendEventCollector created (mode={}, buffer_size={})",
                 static_cast<int>(runtime.configs.frontend_collector.mode),
                 runtime.configs.frontend_collector.buffer_size);

    runtime.initialized = true;
    OdbUtils::odbSetStatusColor(runtime.frontendIndex, runtime.configs.frontend.ready_color);
    return SUCCESS;
}

INT begin_of_run(INT, char *error) {
    try {
        auto& runtime = frontend::runtime::Runtime::instance();
        auto& cfgs = runtime.configs;

        if (!runtime.initialized || !runtime.controller) {
            std::strcpy(error, "System not initialized");
            return FE_ERR_HW;
        }

        std::string err;
        if (!runtime.refreshConfigs(err)) {
            std::snprintf(error, 256, "Failed to refresh configs: %s", err.c_str());
            return FE_ERR_ODB;
        }

        // --- Apply SAMPIC controller configs
        runtime.controller->setSystemSettings(cfgs.system);
        runtime.controller->setControllerConfig(cfgs.controller);
        runtime.controller->setCollectorConfig(cfgs.collector);

        if (runtime.controller->applySettings() != 0) {
            std::strcpy(error, "Failed to apply SAMPIC settings");
            return FE_ERR_HW;
        }

        // --- Apply FrontendEventCollector configs (if available)
        if (runtime.collector) {
            runtime.collector->setConfig(cfgs.frontend_collector);
            if (runtime.collector->applySettings() != 0) {
                std::strcpy(error, "Failed to apply frontend collector settings");
                return FE_ERR_HW;
            }
        } else {
            spdlog::warn("FrontendEventCollector missing during begin_of_run()");
        }

        // --- Start everything
        runtime.controller->startCollector();
        if (runtime.controller->startRun() != 0) {
            std::strcpy(error, "Failed to start SAMPIC run");
            return FE_ERR_HW;
        }

        if (runtime.collector)
            runtime.collector->start();

        spdlog::info("FrontendEventCollector started.");
        runtime.lastEventTimestamp = std::chrono::steady_clock::time_point::min();
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
        auto& runtime = frontend::runtime::Runtime::instance();
        if (runtime.collector)
            runtime.collector->stop();
        if (runtime.controller) {
            runtime.controller->stopCollector();
            runtime.controller->stopRun();
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
        auto& runtime = frontend::runtime::Runtime::instance();
        if (runtime.collector) runtime.collector->stop();
        if (runtime.controller) {
            runtime.controller->stopCollector();
            runtime.controller->stopRun();
            runtime.controller->cleanup();
        }
    } catch (...) {}
    frontend::runtime::Runtime::instance().reset();
    return SUCCESS;
}

// ======================================================================
// Polling
// ======================================================================
INT poll_event(INT, INT, BOOL test) {
    auto& runtime = frontend::runtime::Runtime::instance();
    if (!runtime.initialized || !runtime.collector)
        return test ? FALSE : 0;

    auto now = std::chrono::steady_clock::now();
    if (now - runtime.lastPollTime < runtime.pollingInterval)
        return test ? FALSE : 0;

    runtime.lastPollTime = now;
    if (runtime.collector->buffer().hasNewSince(runtime.lastEventTimestamp))
        return TRUE;

    return test ? FALSE : 0;
}

INT interrupt_configure(INT, INT, POINTER_T) { return SUCCESS; }

// ======================================================================
// Readout (FrontendEvent → multiple banks)
// ======================================================================
INT read_sampic_event(char *pevent, INT)
{
    auto& runtime = frontend::runtime::Runtime::instance();
    if (!runtime.initialized || !runtime.collector)
        return 0;

    const auto t_start = std::chrono::steady_clock::now();

    auto& fbuf = runtime.collector->buffer();
    const auto new_events = fbuf.getSince(runtime.lastEventTimestamp);
    if (new_events.empty())
        return 0;

    bk_init32(pevent);

    for (size_t i = 0; i < new_events.size(); ++i) {
        const auto t_evt_start = std::chrono::steady_clock::now();
        const auto& fev = new_events[i];

        for (const auto& bank : fev->banks()) {
            const std::string bank_name = runtime.makeBankName(bank->bankPrefix());
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

    runtime.lastEventTimestamp = new_events.back()->timestamp();

    const int total_size = bk_size(pevent);
    const auto t_end = std::chrono::steady_clock::now();
    const auto dur_total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();

    spdlog::debug("read_sampic_event: wrote {} FrontendEvents, total MIDAS size={} B ({} µs)",
                  new_events.size(), total_size, dur_total_us);

    if (runtime.collector) {
        runtime.collector->buffer().pruneUpTo(runtime.lastEventTimestamp);
    }

    return total_size;
}
