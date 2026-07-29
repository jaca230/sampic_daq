#include "playground_runtime.h"
#include "integration/sampic/config/sampic_collector_config.h"
#include "processing/sampic_processing/config/frontend_event_collector_config.h"
#include "integration/sampic/collector/modes/simulator/simulator_config.h"
#include "processing/sampic_processing/collector/modes/default/default_config.h"

#include <spdlog/spdlog.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) try {
    // Configure logging
    spdlog::set_pattern("%H:%M:%S.%f [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);

    spdlog::info("========================================");
    spdlog::info("   SAMPIC DAQ Playground Benchmark");
    spdlog::info("========================================");

    // Parse duration from command line (default 10 seconds)
    int duration_sec = 10;
    if (argc > 1) {
        duration_sec = std::atoi(argv[1]);
        if (duration_sec <= 0) {
            spdlog::error("Invalid duration: {}", argv[1]);
            return EXIT_FAILURE;
        }
    }

    // ------------------------------------------------------------------
    // Configure SAMPIC Simulator
    // ------------------------------------------------------------------
    SampicCollectorConfig sampic_cfg;
    sampic_cfg.mode = "simulator";
    sampic_cfg.buffer_size = 128;
    sampic_cfg.sleep_time_us = 0;  // No sleep - max speed

    SampicCollectorModeSimulatorConfig sim;
    sim.events_per_cycle = 1;
    sim.hits_per_event = 1;
    sim.waveform_length = 64;
    sim.simulate_read_time_us = 0;
    sim.baseline_level = 0.05;
    sim.signal_amplitude = 0.8;
    sim.hit_time_step_ns = 5.0;
    sim.inter_event_gap_ns = 1000.0;  // 1 us gap between events
    sim.start_timestamp_ns = 0.0;
    sim.tot_value_ns = 120.0;

    // ------------------------------------------------------------------
    // Configure Frontend Collector
    // ------------------------------------------------------------------
    FrontendEventCollectorConfig frontend_cfg;
    frontend_cfg.mode = "default";
    frontend_cfg.buffer_size = 512;
    frontend_cfg.sleep_time_us = 0;  // No sleep - max speed

    FrontendCollectorModeDefaultConfig fe;
    fe.time_window_ns = 500.0;
    fe.finalize_after_ms = 1.0;
    fe.wait_timeout_ms = 1000;
    fe.data_bank_prefix = "AD";
    fe.event_timing_bank_prefix = "AT";
    fe.collector_timing_bank_prefix = "AC";

    // Disable diagnostics for max performance
    frontend_cfg.diagnostics.enabled = false;

    // ------------------------------------------------------------------
    // Print Configuration
    // ------------------------------------------------------------------
    spdlog::info("Configuration:");
    spdlog::info("");
    spdlog::info("SAMPIC Collector:");
    spdlog::info("  Mode:               SIMULATOR");
    spdlog::info("  Buffer size:        {}", sampic_cfg.buffer_size);
    spdlog::info("  Sleep time:         {} us", sampic_cfg.sleep_time_us);
    spdlog::info("");
    spdlog::info("SAMPIC Simulator Mode:");
    spdlog::info("  Events/cycle:       {}", sim.events_per_cycle);
    spdlog::info("  Hits/event:         {}", sim.hits_per_event);
    spdlog::info("  Waveform length:    {}", sim.waveform_length);
    spdlog::info("  Simulate read time: {} us", sim.simulate_read_time_us);
    spdlog::info("  Baseline level:     {}", sim.baseline_level);
    spdlog::info("  Signal amplitude:   {}", sim.signal_amplitude);
    spdlog::info("  Hit time step:      {} ns", sim.hit_time_step_ns);
    spdlog::info("  Inter-event gap:    {} ns", sim.inter_event_gap_ns);
    spdlog::info("  Start timestamp:    {} ns", sim.start_timestamp_ns);
    spdlog::info("  TOT value:          {} ns", sim.tot_value_ns);
    spdlog::info("");
    spdlog::info("Frontend Collector:");
    spdlog::info("  Mode:               DEFAULT");
    spdlog::info("  Buffer size:        {}", frontend_cfg.buffer_size);
    spdlog::info("  Sleep time:         {} us", frontend_cfg.sleep_time_us);
    spdlog::info("");
    spdlog::info("Frontend Default Mode:");
    spdlog::info("  Time window:        {} ns", fe.time_window_ns);
    spdlog::info("  Finalize after:     {} ms", fe.finalize_after_ms);
    spdlog::info("  Wait timeout:       {} ms", fe.wait_timeout_ms);
    spdlog::info("  Data bank prefix:   {}", fe.data_bank_prefix);
    spdlog::info("  Event timing pfx:   {}", fe.event_timing_bank_prefix);
    spdlog::info("  Collector timing:   {}", fe.collector_timing_bank_prefix);
    spdlog::info("");
    spdlog::info("Run duration:         {} seconds", duration_sec);
    spdlog::info("");
    spdlog::info("========================================");

    // ------------------------------------------------------------------
    // Run Benchmark
    // ------------------------------------------------------------------
    PlaygroundRuntime runtime(sampic_cfg, sim, frontend_cfg, fe);
    runtime.run(std::chrono::seconds(duration_sec));

    spdlog::info("Benchmark complete!");
    return EXIT_SUCCESS;

} catch (const std::exception& ex) {
    spdlog::error("Fatal error: {}", ex.what());
    return EXIT_FAILURE;
}
