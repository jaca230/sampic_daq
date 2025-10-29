# SAMPIC Playground Summary

## What We Built

A **standalone DAQ benchmark tool** that runs the complete SAMPIC→Frontend→Logger pipeline without MIDAS.

### Architecture

```
Thread 1: SampicCollector (simulator)
    ↓ SampicEventBuffer
Thread 2: FrontendEventCollector
    ↓ FrontendEventBuffer
Thread 3: FakeMidasLogger
```

## Current Performance (commit 8b6d1cf)

Running `./scripts/run.sh 5`:

```
Configuration:
  SAMPIC buffer:      128
  Frontend buffer:    512
  Hits per event:     1
  Waveform length:    64
  Inter-event gap:    1000 ns
  Time window:        500 ns

Results:
  Event rate:  1047.5 kHz  (1.05 MHz!)
  Data rate:   455.56 MB/s
```

This is **12-15x faster** than the 70-85 kHz observed with full MIDAS, showing that:
1. The DAQ pipeline itself is very fast
2. MIDAS framework adds significant overhead
3. There's plenty of headroom for optimization

## Usage

### Build
```bash
cd scripts/tools/sampic_playground
./scripts/build.sh
```

### Run
```bash
# 10 seconds (default)
./scripts/run.sh

# Custom duration
./scripts/run.sh 30
```

### Modify Configuration
Edit `src/main.cpp` to change:
- Buffer sizes
- Simulator parameters
- Collector settings
- Event/hit parameters

## Testing Optimizations

When you make a code change:
1. Rebuild playground: `./scripts/build.sh`
2. Run benchmark: `./scripts/run.sh 10`
3. Compare event/data rates

Example workflow:
```bash
# Baseline
./scripts/run.sh 10
# Note: 1047 kHz baseline

# Make code changes...
# (edit parent project sources)

# Test
./scripts/build.sh
./scripts/run.sh 10
# Compare to baseline
```

## Key Files

- **src/main.cpp** - Entry point, configuration
- **src/playground_runtime.cpp** - Pipeline orchestrator
- **src/fake_midas_logger.cpp** - Simulated MIDAS writer
- **CMakeLists.txt** - Build configuration
- **scripts/build.sh** - Build script
- **scripts/run.sh** - Run script

## Benefits

1. **Fast iteration** - No MIDAS setup required
2. **Reproducible** - Same config every run
3. **Measurable** - Direct throughput numbers
4. **Portable** - Runs anywhere with the DAQ code
5. **Real code** - Uses actual DAQ classes, not mocks
