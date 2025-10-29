# MIDAS Frontend Performance Analysis

## Summary of Findings

### Measured Performance
- **MIDAS actual throughput**: ~100 kHz
- **event_writer_loop theoretical max**: 835-1095 kHz
- **Playground (no MIDAS) throughput**: 848 kHz

### Timing Breakdown (per event, from instrumentation)
```
waitAndPop:      0.70-1.95 us  (fetch FrontendEvent from buffer)
rb_get_wp:       0.00 us       (0.00 retries - ring buffer never full!)
compose_event:   0.17-0.50 us  (bank serialization)
rb_increment_wp: 0.00 us       (commit to ring buffer)
─────────────────────────────────
TOTAL:           0.91-2.45 us/event
```

**Theoretical maximum**: 408-1095 kHz (based on event_writer_loop timing alone)

### Key Insight: The Bottleneck is NOT in event_writer_loop

The event_writer_loop can process events **10x faster** than the actual MIDAS throughput!

Evidence:
1. Ring buffer **never fills up** (0.00 retries on rb_get_wp)
2. event_writer_loop measures 0.91-2.45 us/event = 408-1095 kHz theoretical
3. Actual MIDAS rate is only 100 kHz

This proves the bottleneck is **after** rb_increment_wp, somewhere in MIDAS's event consumption pipeline.

## Likely Bottleneck: MIDAS Event Consumer

After rb_increment_wp, MIDAS reads events from the ring buffer using:
```cpp
rb_get_rp(get_event_rbh(index), &p, 10)  // in receive_trigger_event()
```

Then sends events via:
```cpp
rpc_send_event(eq->buffer_handle, pevent,
               pevent->data_size + sizeof(EVENT_HEADER),
               BM_WAIT,  // <-- BLOCKING CALL
               rpc_mode);
```

### Possible Causes of 100 kHz Limit

1. **mlogger writing to disk** - Most likely culprit
   - Disk I/O throughput: typical HDD ~100-150 MB/s = ~10k-15k events/sec for large events
   - SSD can handle more, but still limited by filesystem overhead

2. **SYSTEM buffer too small** (default 2MB)
   - If buffer fills up, BM_WAIT blocks in rpc_send_event
   - However, our timing shows ring buffer never fills (0.00 retries)
   - This suggests MIDAS is draining fast enough, but downstream is slow

3. **Network transmission** (if remote logging enabled)
   - Network latency and bandwidth can limit throughput

4. **ODB/History logging overhead**
   - Equipment definition has history logging enabled
   - Each event may trigger ODB updates

## Recommendations

### Test 1: Disable mlogger
If mlogger is running, stop it and test throughput:
```bash
# Stop mlogger if running
killall mlogger
# Run frontend and check event rate
```

### Test 2: Increase SYSTEM buffer size
In ODB, increase /Equipment/SAMPIC*/Settings/Buffer:
```
odbedit
[local]/>cd /Equipment
[local]/Equipment>ls
...
[local]/Equipment>set "SAMPIC 00"/Settings/Buffer 100000000  # 100 MB
```

### Test 3: Disable history logging
Already done in frontend.cpp (FALSE for history logging)

### Test 4: Use dedicated high-performance buffer
Change buffer from "SYSTEM" to a dedicated "BUF" with larger size and no logger attached.

### Test 5: Bypass MIDAS entirely for speed test
Build with -DBYPASS_MIDAS_BUFFER to skip rb_increment_wp and test pure composition speed.

## Optimizations Already Applied

1. ✅ Optimized makeBankName() - eliminated string allocations
2. ✅ Removed dynamic_cast - use virtual writeTo() method
3. ✅ Reduced waitAndPop timeout from 100ms → 10ms
4. ✅ Changed rb_get_wp retry from sleep(5ms) → yield()
5. ✅ Added detailed timing instrumentation

## Next Steps

1. **Identify the actual bottleneck**:
   - Check if mlogger is running: `ps aux | grep mlogger`
   - Check ODB buffer settings: `odbedit` → `/Equipment/SAMPIC*/Settings/Buffer`
   - Check if remote clients are connected: `odbedit` → `scl` command

2. **If mlogger is the bottleneck**:
   - Disable mlogger for speed testing
   - Or configure mlogger to write to RAM disk (/dev/shm)
   - Or use /dev/null output for maximum speed testing

3. **If buffer size is the issue**:
   - Increase SYSTEM buffer to 100MB or more
   - Monitor with `odbedit` → `/System/Clients/*/Buffer statistics`

4. **Ultimate solution for production**:
   - Use faster storage (NVMe SSD, RAM disk)
   - Implement event compression before logging
   - Use network streaming instead of disk logging
   - Batch events before writing (trade latency for throughput)
