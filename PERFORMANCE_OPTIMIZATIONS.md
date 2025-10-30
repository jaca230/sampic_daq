# Performance Optimization Summary

## Branch: feature/performance-improvements

### Performance Results
- **Baseline**: 70-85 kHz (commit 8b6d1cf)
- **After optimizations**: 130-150 kHz (~2x improvement)
- **Playground (no MIDAS)**: 848 kHz
- **Theoretical limit**: ~325 kHz (event_writer_loop capability)

### Key Optimizations Applied

#### 1. Bank Writing Hotpath (commit f77a77a)
- Added virtual `writeTo()` method to avoid dynamic_cast
- Optimized `makeBankName()` to eliminate string allocations
- Multi-slice bank override for efficient memcpy
- Result: +8% speedup in playground

#### 2. Event Writer Loop Delays (commits b3ac9f2, 52db674)
- Reduced waitAndPop timeout: 100ms → 10ms → 1ms
- Changed rb_get_wp retry: sleep(5ms) → yield()
- Result: Eliminated buffer starvation

#### 3. Diagnostic Overhead Removal (commit 8e35474)
- Removed all timing instrumentation from hot path
- Disabled per-cycle diagnostics.produced() (mutex overhead)
- Removed trace/debug logging from compose operations
- Result: Eliminated ~24% overhead from timing measurements

### Bottleneck Analysis (commits f7b49b7, 85c42fd, 8db0346, 1b15350)

Comprehensive instrumentation revealed:
- Collector produces at 280-315 kHz
- event_writer_loop capability: 325 kHz (3 us/event)
- Actual throughput: 130-150 kHz
- Bottleneck: 10ms waitAndPop timeout causing starvation

### Created Assets
- `scripts/tools/sampic_playground`: Standalone benchmark tool
  - Tests DAQ pipeline without MIDAS overhead
  - Measures pure event composition speed
  - Result: 848 kHz baseline

### Remaining Limitations
- MIDAS ring buffer system adds inherent overhead
- Cannot exceed Collector production rate (~300 kHz)
- Further improvements require:
  - Reducing finalize_after_ms timeout in Collector
  - Increasing events_per_cycle in simulator
  - Or bypassing MIDAS entirely for max speed

## Technical Details

### Hot Path Before Optimization
```
Collector → FrontendEventBuffer → event_writer_loop → rb_get_wp →
compose (with dynamic_cast + logging + timing) → rb_increment_wp → MIDAS
```

### Hot Path After Optimization
```
Collector → FrontendEventBuffer → event_writer_loop (1ms wait) → rb_get_wp (yield retry) →
compose (virtual writeTo, no logging, no timing) → rb_increment_wp → MIDAS
```

### Memory/Move Optimizations Already Applied
All SAMPIC collector modes (default, example, simulator) already use:
- `std::unique_ptr` for zero-copy event handling
- `std::move` semantics throughout
- Efficient multi-slice bank data handling

## Build and Test

```bash
# Build
./scripts/build.sh

# Run playground benchmark (no MIDAS)
cd scripts/tools/sampic_playground
./scripts/build.sh && ./scripts/run.sh

# Run MIDAS frontend
cd ../../..
./scripts/run.sh -i 0
```
