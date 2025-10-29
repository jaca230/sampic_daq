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

int rb_get_wp(int handle, void **p, int millisec);
int rb_increment_wp(int handle, int size);

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

    const auto t_start = std::chrono::steady_clock::now();

    const int total_size = compose_frontend_event(pevent, fev, runtime);
    fev->markConsumed(true);
    runtime.lastEventTimestamp = fev->timestamp();

    const auto t_end = std::chrono::steady_clock::now();
    const auto dur_total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();

    spdlog::debug("read_sampic_event: wrote 1 FrontendEvent, total MIDAS size={} B ({} µs)",
                  total_size, dur_total_us);

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

    // Timing statistics
    uint64_t events_processed = 0;
    uint64_t total_wait_pop_us = 0;
    uint64_t total_rb_get_wp_us = 0;
    uint64_t total_compose_us = 0;
    uint64_t total_rb_increment_us = 0;
    uint64_t rb_get_wp_retries = 0;
    auto last_stats_print = std::chrono::steady_clock::now();

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

        // Time: waitAndPop
        auto t0 = std::chrono::steady_clock::now();
        auto fev = runtime.collector->buffer().waitAndPop(std::chrono::milliseconds(10));
        auto t1 = std::chrono::steady_clock::now();
        if (!fev) {
            continue;
        }
        total_wait_pop_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        EVENT_HEADER* pevent = nullptr;
        int status = DB_TIMEOUT;

        // Time: rb_get_wp (with retries)
        auto t2 = std::chrono::steady_clock::now();
        while (!g_event_writer_stop.load()) {
            status = rb_get_wp(rbh, (void**)&pevent, 0);
            if (status == DB_SUCCESS)
                break;

            if (status == DB_TIMEOUT) {
                if (!is_readout_thread_enabled() || g_event_writer_stop.load())
                    break;
                rb_get_wp_retries++;
                std::this_thread::yield();  // Yield instead of sleeping
                continue;
            }

            spdlog::error("event_writer_loop: rb_get_wp failed with status={}", status);
            pevent = nullptr;
            break;
        }
        auto t3 = std::chrono::steady_clock::now();
        total_rb_get_wp_us += std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        if (!pevent) {
            spdlog::warn("event_writer_loop: dropping FrontendEvent due to ring buffer failure");
            continue;
        }

        bm_compose_event_threadsafe(pevent,
                                    equipment[0].info.event_id,
                                    equipment[0].info.trigger_mask,
                                    0,
                                    &equipment[0].serial_number);

        // Time: compose_frontend_event (bank serialization)
        auto t4 = std::chrono::steady_clock::now();
        auto* payload = reinterpret_cast<char*>(pevent + 1);
        const int total_size = compose_frontend_event(payload, fev, runtime);
        pevent->data_size = total_size;
        auto t5 = std::chrono::steady_clock::now();
        total_compose_us += std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();

        // Time: rb_increment_wp
        auto t6 = std::chrono::steady_clock::now();
        rb_increment_wp(rbh, sizeof(EVENT_HEADER) + pevent->data_size);
        auto t7 = std::chrono::steady_clock::now();
        total_rb_increment_us += std::chrono::duration_cast<std::chrono::microseconds>(t7 - t6).count();

        runtime.lastEventTimestamp = fev->timestamp();
        runtime.collector->diagnostics().consumed(1,
                                                  runtime.collector->buffer().size());

        events_processed++;

        // Print timing stats every 10 seconds
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_print).count() >= 10) {
            if (events_processed > 0) {
                double avg_wait_pop = total_wait_pop_us / (double)events_processed;
                double avg_rb_get_wp = total_rb_get_wp_us / (double)events_processed;
                double avg_compose = total_compose_us / (double)events_processed;
                double avg_rb_increment = total_rb_increment_us / (double)events_processed;
                double avg_retries = rb_get_wp_retries / (double)events_processed;

                // Calculate actual event rate
                double elapsed_s = std::chrono::duration<double>(now - last_stats_print).count();
                double actual_rate_khz = (events_processed / elapsed_s) / 1000.0;

                // Get buffer status
                size_t buffer_size = runtime.collector->buffer().size();

                spdlog::info("=== event_writer_loop timing ({} events in {:.1f}s = {:.1f} kHz) ===",
                            events_processed, elapsed_s, actual_rate_khz);
                spdlog::info("  waitAndPop:      {:.2f} us", avg_wait_pop);
                spdlog::info("  rb_get_wp:       {:.2f} us (avg {:.2f} retries)", avg_rb_get_wp, avg_retries);
                spdlog::info("  compose_event:   {:.2f} us", avg_compose);
                spdlog::info("  rb_increment_wp: {:.2f} us", avg_rb_increment);
                spdlog::info("  TOTAL:           {:.2f} us/event ({:.1f} kHz theoretical max)",
                            avg_wait_pop + avg_rb_get_wp + avg_compose + avg_rb_increment,
                            1000.0 / (avg_wait_pop + avg_rb_get_wp + avg_compose + avg_rb_increment));
                spdlog::info("  FrontendEventBuffer: {} events queued", buffer_size);

                // CRITICAL: Check if buffer is often empty
                if (buffer_size < 10) {
                    spdlog::warn("  ^^^ BOTTLENECK: Buffer nearly empty ({} events) - PRODUCER is slow! (Collector not producing fast enough)", buffer_size);
                } else if (buffer_size > 5000) {
                    spdlog::warn("  ^^^ BOTTLENECK: Buffer very full ({} events) - CONSUMER is slow! (event_writer_loop not draining fast enough)", buffer_size);
                }

                // Also print collector production rate for comparison
                if (runtime.collector) {
                    // Force diagnostics print by calling consumed which triggers maybe_log
                    runtime.collector->diagnostics().consumed(0, buffer_size);
                }
            }

            // Reset counters
            events_processed = 0;
            total_wait_pop_us = 0;
            total_rb_get_wp_us = 0;
            total_compose_us = 0;
            total_rb_increment_us = 0;
            rb_get_wp_retries = 0;
            last_stats_print = now;
        }
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
