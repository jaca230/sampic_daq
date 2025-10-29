# SAMPIC DAQ Playground

A standalone benchmark tool that mimics the full MIDAS DAQ pipeline without requiring MIDAS.

## Purpose

This tool allows you to:
- **Test DAQ performance** without running the full MIDAS framework
- **Benchmark optimizations** by running controlled tests
- **Profile code changes** with consistent, reproducible workloads
- **Measure event/data rates** directly

## Architecture

The playground implements the complete three-stage DAQ pipeline:

```
┌─────────────────────┐
│ SampicCollector     │  (Simulator mode - generates synthetic events)
│  (Thread 1)         │
└──────────┬──────────┘
           │ SampicEventBuffer
           ▼
┌─────────────────────┐
│ FrontendCollector   │  (Groups hits into frontend events)
│  (Thread 2)         │
└──────────┬──────────┘
           │ FrontendEventBuffer
           ▼
┌─────────────────────┐
│ FakeMidasLogger     │  (Consumes events, measures throughput)
│  (Thread 3)         │
└─────────────────────┘
```

## Building

```bash
cd scripts/tools/sampic_playground
./scripts/build.sh
```

## Running

```bash
# Run for 10 seconds (default)
./scripts/run.sh

# Run for custom duration (seconds)
./scripts/run.sh 30
```

## Configuration

Edit [`src/main.cpp`](src/main.cpp) to modify:
- **Buffer sizes**
- **Lock-free vs mutex buffers** (`use_lockfree_buffers`)
- **Simulator parameters** (hits/event, waveform length, timing)
- **Collector parameters** (time windows, finalization timeout)

## Output

The tool reports:
- **Event rate** (kHz)
- **Data rate** (MB/s)
- **Total events/bytes** logged
- **Real-time updates** every second

Example output:
```
========================================
   SAMPIC DAQ Playground Benchmark
========================================
Configuration:
  SAMPIC buffer:      128 (lockfree=false)
  Frontend buffer:    512 (lockfree=false)
  Hits per event:     1
  Waveform length:    64
  Inter-event gap:    1000 ns
  Time window:        500 ns
  Run duration:       10 seconds

Running for 10 seconds...
Rate: 85.2 kHz, 32.1 MB/s
Rate: 86.1 kHz, 32.5 MB/s
...
=== Final Statistics ===
Total events logged: 856234
Total bytes logged:  322453184 (307.56 MB)
Event rate:          85.6 kHz
Data rate:           30.76 MB/s
========================
```

## Testing Optimizations

To test an optimization:

1. Make changes to the main codebase
2. Rebuild the playground: `./scripts/build.sh`
3. Run benchmark: `./scripts/run.sh 30`
4. Compare event/data rates

### Example: Testing Lock-Free Buffers

In `src/main.cpp`, change:
```cpp
sampic_cfg.use_lockfree_buffers = true;
frontend_cfg.use_lockfree_buffers = true;
```

Then rebuild and run to compare performance.

## Notes

- Uses the **exact same code** as the main DAQ (just recompiled)
- Simulator mode only - no hardware required
- Optimized build (`-O3 -march=native`)
- No MIDAS overhead - pure DAQ pipeline performance
