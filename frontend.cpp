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
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <unistd.h>

// MIDAS
#include "midas.h"
#include "mfe.h"

// Project: ODB + logging + FE config
#include "integration/midas/frontend_config.h"
#include "integration/midas/event_buffer/frontend_event_buffer_flusher.h"
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

static std::thread g_event_writer_thread;
static std::atomic<bool> g_event_writer_stop{false};
static std::atomic<bool> g_event_writer_pause{false};

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
static void event_writer_loop();
static INT compose_frontend_event(char* dest,
                                  const std::shared_ptr<FrontendEvent>& fev,
                                  frontend::runtime::Runtime& runtime);

// ======================================================================
// Equipment definition
// ======================================================================
BOOL equipment_common_overwrite = FALSE;

EQUIPMENT equipment[] = {
    {"SAMPIC %02d",
        { 1, 0,
          "SYSTEM",
          EQ_USER | EQ_EB,
          0,
          "MIDAS",
          TRUE,
          RO_RUNNING,
          100,
          0,
          0,
          TRUE,
          "", "", "", },
        nullptr
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

    create_event_rb(0);
    g_event_writer_stop = false;
    g_event_writer_thread = std::thread(event_writer_loop);

    runtime.initialized = true;
    OdbUtils::odbSetStatusColor(runtime.frontendIndex, runtime.configs.frontend.ready_color);
    return SUCCESS;
}

INT begin_of_run(INT, char *error) {
    try {
        auto& runtime = frontend::runtime::Runtime::instance();
        auto& cfgs = runtime.configs;
        g_event_writer_pause = false;

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

        // Pause regular writer thread so end-of-run flush can serialize safely.
        g_event_writer_pause = true;

        if (runtime.controller) {
            runtime.controller->stopRun();

            // Allow collector pipeline to finalize trailing groups after run stop.
            const auto grace_ms = static_cast<int>(
                runtime.configs.frontend_collector.default_mode.finalize_after_ms +
                runtime.configs.frontend_collector.default_mode.wait_timeout_ms + 50u);
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(50, grace_ms)));

            runtime.controller->stopCollector();
        }
        if (runtime.collector)
            runtime.collector->stop();

        constexpr size_t kMaxEndOfRunFlushEvents = 100000;
        size_t flushed_events = 0;
        if (runtime.collector) {
            integration::midas::FrontendEventBufferFlusher flusher;
            const INT flush_status = flusher.flush(
                runtime.collector->buffer(),
                kMaxEndOfRunFlushEvents,
                1000,
                equipment[0].info.event_id,
                equipment[0].info.trigger_mask,
                &equipment[0].serial_number,
                g_event_writer_stop,
                [&](char* payload, const std::shared_ptr<FrontendEvent>& fev) {
                    return compose_frontend_event(payload, fev, runtime);
                },
                [&](size_t remaining_size) {
                    runtime.collector->diagnostics().consumed(1, remaining_size);
                },
                flushed_events);
            if (flush_status != SUCCESS) {
                std::strcpy(error, "Failed to flush remaining frontend events at end-of-run");
                g_event_writer_pause = false;
                return flush_status;
            }
        }

        spdlog::info("end_of_run: flushed {} remaining FrontendEvent(s)", flushed_events);
        g_event_writer_pause = false;
    } catch (const std::exception& e) {
        std::snprintf(error, 256, "Error during EOR: %s", e.what());
        g_event_writer_pause = false;
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
    g_event_writer_stop = true;
    stop_readout_threads();
    if (g_event_writer_thread.joinable())
        g_event_writer_thread.join();
    frontend::runtime::Runtime::instance().reset();
    return SUCCESS;
}

// ======================================================================
// Polling
// ======================================================================
INT poll_event(INT, INT, BOOL test) {
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

    auto& buffer = runtime.collector->buffer();
    auto opt_event = buffer.pop();
    if (!opt_event)
        return 0;

    const auto& fev = *opt_event;
    if (!fev)
        return 0;

    const int total_size = compose_frontend_event(pevent, fev, runtime);
    fev->markConsumed(true);
    runtime.lastEventTimestamp = fev->timestamp();

    if (runtime.collector) {
        runtime.collector->diagnostics().consumed(1,
                                                  runtime.collector->buffer().size());
    }

    return total_size;
}

static void event_writer_loop()
{
    const int thread_index = 0;
    signal_readout_thread_active(thread_index, TRUE);
    integration::midas::FrontendEventBufferFlusher flusher(thread_index);

    while (is_readout_thread_enabled() && !g_event_writer_stop.load()) {
        if (!readout_enabled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (g_event_writer_pause.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto& runtime = frontend::runtime::Runtime::instance();
        if (!runtime.collector) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto fev = runtime.collector->buffer().waitAndPop(std::chrono::milliseconds(10));
        if (!fev) {
            continue;
        }

        if (!flusher.writeOne(
                fev,
                equipment[0].info.event_id,
                equipment[0].info.trigger_mask,
                &equipment[0].serial_number,
                g_event_writer_stop,
                [&](char* payload, const std::shared_ptr<FrontendEvent>& event) {
                    return compose_frontend_event(payload, event, runtime);
                },
                1000)) {
            spdlog::warn("event_writer_loop: dropping FrontendEvent due to ring buffer failure");
            continue;
        }

        runtime.lastEventTimestamp = fev->timestamp();
        runtime.collector->diagnostics().consumed(1,
                                                  runtime.collector->buffer().size());
    }

    signal_readout_thread_active(thread_index, FALSE);
}
static INT compose_frontend_event(char* dest,
                                  const std::shared_ptr<FrontendEvent>& fev,
                                  frontend::runtime::Runtime& runtime)
{
    if (!dest || !fev)
        return 0;

    spdlog::trace("compose_frontend_event: FrontendEvent has {} banks", fev->numBanks());

    bk_init32(dest);

    size_t bank_index = 0;
    for (const auto& bank : fev->banks()) {
        if (!bank)
            continue;

        const std::string bank_name = runtime.makeBankName(bank->bankPrefix());
        uint8_t* pdata = nullptr;
        bk_create(dest, bank_name.c_str(), TID_UINT8, (void**)&pdata);
        uint8_t* const pstart = pdata;

        // Optimized: Use virtual writeTo() instead of dynamic_cast + branching
        bank->writeTo(pdata);
        pdata += bank->size();

        bk_close(dest, pdata);
        spdlog::trace("FrontendEvent bank[{}] → wrote {} ({} bytes)",
                      bank_index++, bank_name, static_cast<int>(pdata - pstart));
    }

    return bk_size(dest);
}
