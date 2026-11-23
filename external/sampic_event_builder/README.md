# SAMPIC event builder

The builder runs `collection -> optional cuts -> finalization`, preserving raw
signed int16 samples. Crate frontends use `vendor_raw_passthrough` and publish
`SRnn` waveform banks with `SAnn` acquisition timing.

## Build and run

From the repository root with MIDAS and the crate frontend dependencies installed:

```bash
export MIDASSYS=/home/jack/software/midas
./scripts/build.sh -j 4
./external/sampic_event_builder/scripts/build.sh
./external/sampic_event_builder/scripts/run.sh
```

The launcher forwards frontend arguments and respects `MIDAS_EXPTAB`,
`MIDAS_EXPT_NAME`, and `SAMPIC_EVENT_BUILDER_BUILD_DIR`.

Each crate's `/Equipment/SAMPIC nn/Common/Buffer` must be `SBUFnn`, with event
ID 1. The builder subscribes with `GET_ALL` to the comma-separated
`/Equipment/SAMPIC Builder/Settings/Input buffers`, defaulting to
`SBUF00,SBUF01,...` for `Settings/Expected crates`. Reads are polled fairly.
Input buffers must be unique, separate from output, and match the crate count.

The builder publishes event ID 2 to `SYSTEM`. Configure the logger for event ID
2 so raw crate events are not duplicated in the output. Frontends preserve ODB
Common settings; configure them before startup. The simulator launcher initializes
the Common settings for both crates and builder.

Input and output event limits are 16 MiB. Output size is checked before copying,
including the MIDAS event header, bank headers, and 8-byte bank padding. An
oversized event produces an explicit failure; it is not split automatically.
Time-grouped events can exceed this limit.

## Collection and cuts

`Settings/Collection strategy` accepts `time_grouping` (or `default`),
`fixed_time_bins`, or `periodic_readout`. Parameters live under `Time grouping`,
`Fixed time bins`, and `Periodic readout`. Set `Expected crates` to the number
of source streams. External-trigger association is not supported by this input.

Cuts are disabled by default. `Settings/Selection/Enabled` enables the existing
punch-through positron rejection: all configured planes occupied and mean
integrated pulse per hit below `Maximum punch integral per hit`. Plane count
and channels per plane are configurable in the same settings directory.

## BD00 waveform format

Each retained event has one `BD00` bank of type `TID_UINT8`. Version 3 uses
packed little-endian fields; older product versions are rejected.

The event header is 28 bytes:

| Offset | Field | Type |
|---:|---|---|
| 0 | magic (`0x31424553`, bytes `SEB1`) | uint32 |
| 4 | version (`3`) | uint16 |
| 6 | reserved (`0`) | uint16 |
| 8 | waveform_count | uint32 |
| 12 | start_ns | uint64 |
| 20 | end_ns | uint64 |

Each waveform has an 18-byte header followed by `sample_count` int16 samples:

| Offset | Field | Type |
|---:|---|---|
| 0 | crate | uint16 |
| 2 | board | uint16 |
| 4 | channel | uint16 |
| 6 | timestamp_ns | uint64 |
| 14 | sample_count | uint32 |

A 64-sample waveform uses 146 bytes, plus the shared event header and MIDAS
framing. Samples are raw ADC counts. Time grouping stores the first and last hit
hardware timestamps (inclusive end); fixed bins store hardware bin boundaries
(exclusive end). Periodic readout stores host steady-clock intervals relative
to its collection origin (exclusive end). Waveform timestamps always use
hardware ns. Event bounds are not wall-clock timestamps.

## Timing banks

Timing is enabled by default. Raw passthrough emits `SAnn` per acquisition;
other frontend modes retain their existing `STnn` and `SCnn` layouts.
The builder consumes `SAnn` and emits separate diagnostic events on event ID 2
in `SYSTEM`, containing `BTnn` summaries and `BC00` collector timing, without
`BD00`. Summaries are independent of waveform grouping and cuts.

All timing records start with four-byte magic, uint16 version (1), and uint16
record size, followed by uint64 fields in the order below. Sizes exclude MIDAS
framing. Definitions are in `include/sampic_event_builder/diagnostics/timing_format.h`.

| Bank | Bytes | Fields after the format header |
|---|---:|---|
| `SAnn` (`SAT1`) | 72 | Acquisition ID, source-host steady-clock completion ns, hit count, prepare/read/decode/total us, retries |
| `BTnn` (`BTS1`) | 128 | Cycle ID, read count, hit count, prepare/read/decode/total sums (us), corresponding maxima (us), retries sum/max, first/last acquisition IDs |
| `BC00` (`BCT1`) | 128 | Cycle ID, cycle start ns since run start, input events/hits, output physics events/hits, selected/rejected events, input-read/build/cuts/finalize/output-wait/total ns, flags |

`BC00` flag bit 0 marks an end-of-run flush. Empty idle polls emit no diagnostics.
Total cycle time ends before constructing/sending diagnostics; output-wait times
physics submission. Host timestamps are not hardware timestamps and should not
be compared across hosts. Acquisition read durations may include vendor retries;
individual retries and empty hardware polls are not recorded. IDs reset each run.
Missing/malformed timing, acquisition ID gaps, and SR/SA hit-count mismatches are
fatal input errors.

Source settings are `vendor_raw_passthrough/acquisition_timing_bank_enabled` and
`acquisition_timing_bank_prefix` (default `SA`, expected by the builder). To disable
timing, disable the source setting on each crate and set builder
`Settings/Timing/Enabled` false before starting the builder.

## Simulator, tests, and benchmarks

From the repository root after building both frontends:

```bash
# Two simulated crates and a local MIDAS webpage; Ctrl+C stops everything.
/usr/bin/python3 external/sampic_event_builder/tests/scripts/run_simulator.py --crates 2 --port 8080

# Record an uncompressed MIDAS file using the logger; validate its contents.
/usr/bin/python3 external/sampic_event_builder/tests/scripts/record_simulator.py --duration 10 --rate-hz 100

# Two-crate software chain, including MIDAS transport and an output sink.
/usr/bin/python3 external/sampic_event_builder/tests/benchmarks/run_midas_benchmark.py \
  --crates 2 --duration 10 --rate-hz 8000 --hits-per-event 256

# Unit tests and software benchmarks, including MIDAS size tests when enabled.
./external/sampic_event_builder/tests/scripts/build.sh

# Fatal frontend queue-overflow checks.
/usr/bin/python3 external/sampic_event_builder/tests/scripts/test_queue_overflow.py
```

The webpage is http://localhost:8080. Launchers use isolated experiments under
`.artifacts/` and leave the normal ODB unchanged. Simulator waveforms contain
pulses, Gaussian noise, amplitude variation and trigger jitter. Cuts default off;
`--select` enables them. The recording script saves counters, an ODB snapshot and
a QA manifest alongside the file. No manifests are required for production.

Benchmarks exclude disk logging. With cuts disabled, finalized/input hit counts
and received/summarized acquisition counts must match after STOP. Queue overflow
or malformed input fails the benchmark. This accounting check does not compare
every output sample; the recording script additionally validates bank layouts.
Benchmark sources are under `tests/benchmarks/`, scripts under `tests/scripts/`.
Workers have no default CPU affinity; the optional core-hint API remains available.
