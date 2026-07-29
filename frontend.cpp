// ======================================================================
// SAMPIC Frontend (ODB-driven; uses SampicController + FrontendEventCollector)
// ======================================================================

#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <atomic>

// MIDAS
#include "midas.h"
#include "mfe.h"

// Project: ODB + logging + FE config
#include "integration/midas/frontend_config.h"
#include "integration/midas/frontend_odb_paths.h"
#include "integration/midas/frontend_runtime.h"
#include "integration/midas/frontend_support.h"
#include "integration/midas/odb/odb_utils.h"
#include "integration/midas/odb/odb_manager.h"
#include <spdlog/spdlog.h>

// Project: SAMPIC controller + configs
#include "integration/sampic/config/sampic_controller_config.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "integration/sampic/controller/sampic_controller.h"

// Project: Frontend collector (higher-level grouping)
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "processing/sampic_processing/collector/frontend_event_collector.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"

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
static std::atomic<bool> g_shutdown_started{false};

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
static void start_event_writer();
static void stop_event_writer();
static void shutdown_frontend_runtime(const char* reason);
static void request_fatal_shutdown(const char* reason);

// ======================================================================
// Equipment definition
// ======================================================================
BOOL equipment_common_overwrite = TRUE;

EQUIPMENT equipment[] = {
    {"SAMPIC %02d",
        { 1, 0,
          "SYSTEM",
          EQ_USER,
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
        runtime.configs.frontend_collector,
        OdbManager{},
        frontend::odb::make_path(
            runtime.settingsPath,
            frontend::odb::Section::FrontendEventCollector) + "/modes"
    );

    spdlog::info("FrontendEventCollector created (mode={}, buffer_size={})",
                 runtime.configs.frontend_collector.mode,
                 runtime.configs.frontend_collector.buffer_size);

    create_event_rb(0);
    start_event_writer();

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
            request_fatal_shutdown(error);
            return FE_ERR_ODB;
        }

        // --- Apply SAMPIC controller configs
        OdbManager odb;
        runtime.controller->setControllerConfig(cfgs.controller, odb);
        runtime.controller->setCollectorConfig(cfgs.collector);

        if (runtime.controller->applySettings(odb) != 0) {
            std::strcpy(error, "Failed to apply SAMPIC settings");
            request_fatal_shutdown(error);
            return FE_ERR_HW;
        }

        // --- Apply FrontendEventCollector configs (if available)
        if (runtime.collector) {
            runtime.collector->setConfig(cfgs.frontend_collector);
            if (runtime.collector->applySettings(odb) != 0) {
                std::strcpy(error, "Failed to apply frontend collector settings");
                request_fatal_shutdown(error);
                return FE_ERR_HW;
            }
        } else {
            std::strcpy(error, "FrontendEventCollector missing during begin_of_run()");
            request_fatal_shutdown(error);
            return FE_ERR_HW;
        }

        // --- Start everything
        start_event_writer();
        runtime.controller->startCollector();
        const int start_status = runtime.controller->startRun();
        if (start_status != 0) {
            std::snprintf(error, 256, "Failed to start SAMPIC run (err=%d)", start_status);
            request_fatal_shutdown(error);
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
        request_fatal_shutdown(error);
        return FE_ERR_HW;
    } catch (...) {
        std::strcpy(error, "Unknown exception in begin_of_run");
        spdlog::error("begin_of_run() unknown exception");
        request_fatal_shutdown(error);
        return FE_ERR_HW;
    }
}



INT end_of_run(INT, char *error) {
    try {
        auto& runtime = frontend::runtime::Runtime::instance();

        // mfe disables readout before invoking this callback. Fully stop the
        // asynchronous writer so it cannot race the synchronous EOR flush.
        stop_event_writer();

        int stop_status = 0;
        if (runtime.controller) {
            // Stop capture first while the SAMPIC collector is still able to
            // finish its current vendor-library read, then join that worker.
            stop_status = runtime.controller->stopRun();
            runtime.controller->stopCollector();
        }

        if (runtime.collector && !runtime.collector->drain()) {
            std::strcpy(
                error,
                "Failed to drain SAMPIC events through the frontend collector");
            request_fatal_shutdown(error);
            return FE_ERR_HW;
        }

        constexpr std::size_t kMaxEndOfRunEvents = 100'000;
        if (runtime.collector) {
            const INT flush_status =
                integration::midas::FrontendSupport::flushEndOfRun(
                runtime.collector->buffer(),
                equipment[0],
                event_buffer,
                get_event_rbh(0),
                runtime,
                kMaxEndOfRunEvents);
            if (flush_status != SUCCESS) {
                std::snprintf(
                    error,
                    256,
                    "Failed to flush frontend events at end-of-run "
                    "(status=%d)",
                    flush_status);
                request_fatal_shutdown(error);
                return FE_ERR_HW;
            }
        }

        if (stop_status != 0) {
            std::snprintf(
                error,
                256,
                "Failed to stop SAMPIC run (err=%d)",
                stop_status);
            request_fatal_shutdown(error);
            return FE_ERR_HW;
        }
    } catch (const std::exception& e) {
        std::snprintf(error, 256, "Error during EOR: %s", e.what());
        request_fatal_shutdown(error);
        return FE_ERR_HW;
    }
    return SUCCESS;
}


INT pause_run(INT, char*)  { return SUCCESS; }
INT resume_run(INT, char*) { return SUCCESS; }
INT frontend_loop()        { return SUCCESS; }

INT frontend_exit() {
    shutdown_frontend_runtime("MIDAS frontend exit");
    return SUCCESS;
}

static void shutdown_frontend_runtime(const char* reason) {
    if (g_shutdown_started.exchange(true))
        return;

    spdlog::info("Shutting down frontend runtime: {}", reason ? reason : "unspecified reason");
    g_event_writer_stop = true;
    stop_readout_threads();
    stop_event_writer();

    try {
        auto& runtime = frontend::runtime::Runtime::instance();
        if (runtime.collector)
            runtime.collector->stop();
        if (runtime.controller)
            runtime.controller->cleanup();
    } catch (const std::exception& e) {
        spdlog::error("Exception while shutting down SAMPIC runtime: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown exception while shutting down SAMPIC runtime");
    }

    frontend::runtime::Runtime::instance().reset();
}

static void request_fatal_shutdown(const char* reason) {
    const char* message = reason ? reason : "fatal frontend hardware error";
    spdlog::critical("Fatal frontend error: {}", message);

    auto& runtime = frontend::runtime::Runtime::instance();
    OdbUtils::odbSetStatusColor(runtime.frontendIndex, "redLight");
    OdbUtils::odbSetStatusMessage(runtime.frontendIndex, message);
    shutdown_frontend_runtime(message);

    // fe_stop is the MIDAS mfe scheduler's documented stop switch. Once this
    // transition callback returns, mfe calls frontend_exit() and disconnects.
    fe_stop = 1;
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

    const int total_size =
        integration::midas::FrontendSupport::composeEvent(
            pevent, fev, runtime);
    if (total_size <= 0) {
        buffer.pushFront(fev);
        return 0;
    }

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

    const int rbh = get_event_rbh(thread_index);

    while (is_readout_thread_enabled() && !g_event_writer_stop.load()) {
        if (!readout_enabled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
        if (g_event_writer_stop.load()) {
            runtime.collector->buffer().pushFront(fev);
            break;
        }

        if (!integration::midas::FrontendSupport::writeEventToRing(
                fev,
                equipment[0],
                rbh,
                runtime,
                g_event_writer_stop)) {
            runtime.collector->buffer().pushFront(fev);
            if (!g_event_writer_stop.load()) {
                spdlog::error(
                    "event_writer_loop: restored FrontendEvent after "
                    "ring-buffer failure");
            }
            continue;
        }

        runtime.lastEventTimestamp = fev->timestamp();
        runtime.collector->diagnostics().consumed(1,
                                                  runtime.collector->buffer().size());
    }

    signal_readout_thread_active(thread_index, FALSE);
}

static void start_event_writer() {
    if (g_event_writer_thread.joinable()) {
        return;
    }

    g_event_writer_stop = false;
    g_event_writer_thread = std::thread(event_writer_loop);
}

static void stop_event_writer() {
    g_event_writer_stop = true;
    if (g_event_writer_thread.joinable()) {
        g_event_writer_thread.join();
    }
}
