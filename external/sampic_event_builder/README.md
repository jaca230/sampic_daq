# SAMPIC event builder

The builder runs `collection -> optional cuts -> finalization`, retaining the same corrected
waveforms and timing metadata as compact frontend SD/SH banks. Crate frontends use `vendor_raw_passthrough` and publish
`SRnn` full vendor-hit banks with `SAnn` acquisition timing.

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

`Settings/Collection_strategies/Active strategy` selects `periodic_readout`
(the default), `time_grouping` (or legacy alias `default`), or `fixed_time_bins`.
Each strategy's parameters are registered under its matching child folder,
including inactive strategies. Selection stays under `Settings/Selection`
and is independent of the chosen collector. These replace the previous flat
`Collection strategy`, `Time grouping`, `Fixed time bins`, and `Periodic readout`
paths; update existing ODB setups to the new paths. Settings require a builder
process restart to take effect.

`periodic_readout` accumulates arriving hits until a host steady-clock interval
closes; hardware timestamps do not determine collection. The simulator and QA
launchers default to this mode with a 10 ms period. Periodic readouts always include BD, even with zero hits.
Use `--collection-strategy time_grouping` to test timestamp grouping explicitly.
In periodic mode, input batches accumulate without emitting banks until the
readout deadline. BC/BS summarize the entire window, including all input batches;
each window emits BD, BC and BS, including a header-only BD for empty payloads. End of run flushes the
remaining partial window. The worker uses bounded MIDAS input waits with rotating
source order while idle, and continues draining batches while input is available.
Timestamp-based modes retain their processing-iteration accounting. `Expected crates` sets the source stream count. External-trigger association
is not supported by this input.

Cuts register by stable ID under `Settings/Selection/<id>` and are disabled by
default. Each section has `Enabled` and signed integer `Priority`; lower values
run first, with ID breaking ties. Settings are read when the builder process
starts. `topology` defaults to priority 100 and retains the existing plane/channel
and maximum punch-integral settings. `dummy` defaults to priority 200 and clears the payload of
every `Reject every N events` (default 2) reaching it; it is for QA only.
Cuts operate on the hit payloads of built events, rather than incoming vendor
packets. Later cuts receive surviving hits; empty payloads skip further cuts.
Event headers and collection bounds are always preserved. Counters and the dummy sequence reset
at beginning of run. The old flat `Settings/Selection/Enabled` setting is no
longer used; enable `Settings/Selection/topology/Enabled` instead.

`BS00` (selection summary) is independent of timing enablement and accompanies
each active diagnostic cycle, including the final partial window. It contains per-cycle
**deltas**, not cumulative counters; `cycle_id` joins it to BC/BT when present.
Disabled cuts are included with zero counters, in effective priority/ID order.
Cumulative counts are also published under `Variables/Selection/<id>`.

BS is packed little endian. The 24-byte header is `BSS1` magic (4 bytes), version
uint16 (1), record size uint16 (72), cycle_id uint64, cut_count uint32 and reserved
uint32 (0). Each 72-byte record is cut ID `char[32]` (NUL-padded; lowercase letters,
digits, underscore), enabled uint32 (0/1), priority int32, then uint64 evaluated
events, rejected events, evaluated hits, rejected hits. The hit counters account
for removed waveforms when payloads have different multiplicities. Two registered
cuts cost 168 payload bytes per cycle, plus the MIDAS bank header/padding.

## BD00 waveform format

Each retained event has one `BD00` bank of type `TID_UINT8`. Version 4 uses
packed little-endian fields; older product versions are rejected.

The event header is 28 bytes:

| Offset | Field | Type |
|---:|---|---|
| 0 | magic (`0x31424553`, bytes `SEB1`) | uint32 |
| 4 | version (`4`) | uint16 |
| 6 | reserved (`0`) | uint16 |
| 8 | waveform_count | uint32 |
| 12 | start_ns | uint64 |
| 20 | end_ns | uint64 |

Each hit is a fixed 188-byte record:

| Offset | Field | Bytes |
|---:|---|---:|
| 0 | crate (uint16) | 2 |
| 2 | exact compact SD record (`CompactData`) | 154 |
| 156 | exact compact SH record (`CompactAdvanced`) | 32 |

SD fields include board/channel, correction and quality flags, 64 signed int16
samples, valid sample count, ToT and the original **double** first-cell timestamp.
SH retains physical-cell-zero time (double), SAMPIC timestamps A/B, FPGA timestamp
and trigger-position mask. The shared definitions and conversion functions are
in `include/processing/sampic_processing/collector/banks/sampic_hit_format.h`.
ADC-corrected samples use 0.1 mV/count (divide by 10000 for volts); otherwise
samples use ADC counts. Rounding, clipping and nonfinite flags match direct SD.
Calibration is performed by the vendor library in the crate frontend; the
builder quantizes its corrected samples rather than substituting raw ADC data.

Time grouping still uses integer-nanosecond grouping keys; it does not change
the original fractional hit timestamps stored in BD. Time grouping stores first
and last grouping keys as event bounds (inclusive end); fixed bins store
hardware bin boundaries (exclusive end). Periodic readout stores host
steady-clock intervals relative to its collection origin (exclusive end),
without consulting hardware timestamps for collection. Event bounds are not
wall-clock timestamps.

Raw passthrough `SRnn` version 3 (`SRW3`) carries a 16-byte header (uint32 magic,
uint16 version/header size, uint32 record size/hit count), followed directly by
native vendor `HitStruct` records. Every field and all 64 array elements are
retained. Frontend and builder must use matching vendor headers, layout and
endianness; the default is SAMPIC 3.2. To select 3.1, configure both builds with
`-DSAMPIC_LIBRARY_VERSION=3.1` (or a matching `SAMPIC_SOURCE_ROOT_OVERRIDE`).
Header version, record size and payload length are checked before ingestion.

The frontend bank retains its original vendor event and writes its contiguous
hit array directly into the MIDAS destination. The builder copies each received
bank's hit array once into owned batch storage, because MIDAS reuses its receive
buffer. Hits share ownership of that batch through collection and cuts; there
is no per-hit payload allocation/copy and no intermediate compact waveform.
Optional cuts inspect corrected vendor floats in compact-output units, without
rounding/clipping first; finalization alone performs SD/SH compaction. Collection
metrics still count valid samples at two bytes/sample. A retained hit keeps its
whole input batch alive until all references to that batch are released.

SR versions 1/2 are rejected. The compact BD version remains 4; its disk layout
and timestamp precision are unchanged by the native raw transport.

## Timing banks

Timing is enabled by default. Raw passthrough emits `SAnn` per acquisition;
other frontend modes retain their existing `STnn` and `SCnn` layouts.
The builder consumes `SAnn` and emits separate diagnostic events on event ID 2
in `SYSTEM`, containing `BTnn` summaries and `BC00` collector timing, without
`BD00`. Summaries are independent of waveform grouping and cuts.

All timing records start with four-byte magic, uint16 version (1), and uint16
record size, followed by uint64 fields in the order below. Sizes exclude MIDAS
framing. Bank classes and wire layouts are in `include/sampic_event_builder/banks/`: `data_bank.h` (BD), `acquisition_summary_bank.h` (BT), `collection_timing_bank.h` (BC), and `selection_summary_bank.h` (BS). The input SA format is in `acquisition_timing_bank.h`.

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

Selections mutate the hit payload and always preserve the event envelope. Later
cuts see surviving hits and are skipped when none remain. Legacy BS
`rejected_events` counts payloads modified by that cut; `rejected_hits` counts
removed hits. BC `selected_events` counts unchanged payloads and
`rejected_events` counts modified payloads; both are finalized and written.
The QA recorder defaults to 500 input events/second/crate to provide several
input batches per 10 ms window. Scheduling can still produce an empty window.
