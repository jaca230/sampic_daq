# sampic_tests

A lightweight playground for SAMPIC crate experimentation outside the MIDAS frontend.  The initial utility, `sampic_pulser_rate`, connects directly to the crate, enables the on-board pulser, and measures the resulting acquisition rate.  A second utility, `sampic_crate_smoke`, performs a single acquisition handshake and prints detailed diagnostics for each API call, which is handy when communications fail.

The code is a C++ port of the vendor `sampic_test.c` example with additional pulser-specific configuration and timing diagnostics.  Use this tool to iterate on high-rate settings before shipping changes into the MIDAS frontend.

## Build

```bash
cmake -S scripts/tools/sampic_tests -B scripts/tools/sampic_tests/build
cmake --build scripts/tools/sampic_tests/build
```

This project reuses the main build’s configuration for the vendor driver, so run it from the repository root.

## Usage

```bash
scripts/tools/sampic_tests/scripts/run.sh --pulser -- \
    --ip 192.168.0.4 \
    --port 27015 \
    --period-ticks 6400 \
    --events 500 \
    --threshold 0.1

# or run the multi-mode binary directly:
scripts/tools/sampic_tests/build/bin/sampic_deadtime_scan \
    --mode pulser-rate \
    --ip 192.168.0.4 \
    --port 27015 \
    --period-ticks 6400 \
    --events 500 \
    --threshold 0.1
```

Options:

- `--ip` / `--port` – crate control endpoint (defaults: `192.168.0.4:27015`).
- `--period-ticks` – pulser period in SAMPIC clock ticks (not microseconds). The default of 6400 ≈ 1 µs at 6.4 GHz.
- `--events` – number of decoded events to acquire before stopping (0 = run until `--duration` elapses).
- `--duration` – optional run duration in seconds (0 = no time limit).
- `--threshold` – internal trigger threshold (volts, relative to baseline).
- `--no-calibration` – skip loading calibration files.
- `--calibration-dir` – override calibration lookup directory (default `resources/calib` relative to the current working dir).
- `--quiet` – suppress per-event log messages.

The program prints aggregate statistics (events/second, average hits, decoder timings) plus the last MIDAS-style event summary for debugging.

> **Warning**
>
> The tool talks to the crate directly; do not run it concurrently with the MIDAS frontend.

### Smoke test

```bash
scripts/tools/sampic_tests/scripts/run.sh --smoke -- --ip 192.168.0.4 --port 27015

# or run the binary directly:
scripts/tools/sampic_tests/build/bin/sampic_deadtime_scan --mode crate-smoke \
    --ip 192.168.0.4 --port 27015
```

The smoke test walks through connection, configuration, memory allocation, run start, and a single read attempt.  Each API call is logged with the corresponding error-code mnemonic so you can quickly identify which step failed.

### Channel occupancy sampler (self trigger)

Use the occupancy mode to grab a quick set of self-triggered events, but configure the crate the same way the MIDAS frontend does (explicit board enables instead of `ALL_FE_BOARDs`). Example:

```bash
scripts/tools/sampic_tests/scripts/helpers/channel_occupancy_mode.sh \
  --board 0 --events 200 --json
```

This runs `sampic_deadtime_scan --mode occupancy`, which disables every FEB, enables the requested board, forces `SAMPIC_CHANNEL_SELF_TRIGGER_MODE` with a 0.1 V threshold, and then reports the hit count plus hits-per-event for each `(FEB, Sampic, Channel)` tuple. Pass `--skip-calibration`, `--calibration-dir /path`, `--duration`, `--threshold`, or `--json` to mirror the DAQ defaults while steering to the FEB that’s actually cabled.

### Board probe (experimental)

`scripts/tools/sampic_tests/scripts/helpers/board_probe.sh --ip 192.168.0.4 --mask 0xFF` pokes the control FPGA’s “front-end presence” register and then issues the same raw bus command used in `SAMPIC256CH_OpenCrateConnection()`. It prints what the library reports (`NbOfFeBoards`) plus any additional FEB paths discovered when we force different bitmasks. Use this to debug why only one board responds: the helper cycles through all four possible paths and shows which ones ack the broadcast.

### Deadtime scan harness

The `sampic_deadtime_scan` binary automates the 3-parameter scan (pulser period, digitization rate, enabled channel count) needed for the deadtime study. It ingests a JSON configuration file that defines the parameter space, timing of each sample, retry policy, and the log destination. Each parameter combination is logged immediately after it finishes so the scan can be resumed safely.

Build it via the main `cmake` invocation above, then run:

```bash
scripts/tools/sampic_tests/scripts/run.sh --deadtime -- \
  --config scripts/tools/sampic_tests/config/deadtime_scan.default.json

# or run directly:
scripts/tools/sampic_tests/build/bin/sampic_deadtime_scan --mode deadtime \
  --config scripts/tools/sampic_tests/config/deadtime_scan.default.json
```

Key points:

- `scripts/tools/sampic_tests/config/deadtime_scan.default.json` contains the first-pass space (9 pulser periods × 7 channel populations × 5 digitizer rates). Adjust the arrays or add explicit `channel_sets` entries to target different subsets (IDs are 0–63 on FEB0).
- Pulser periods are specified directly in auto-pulser ticks (1 tick ≈ 1 µs). They must stay within the hardware’s `[2, 65535]` range; the tool will abort early if a combo exceeds that so you can trim the list before a long scan.
- Runtime timing is configured in the `timing` block (default: 3 s samples, 5 per combo, zero gap within a run, optional cooldown only between combos). Retry behavior (five start attempts with exponential backoff) lives in `start_retry`.
- Each parameter combo is executed as a single run; the tool simply logs rolling sample slices every few seconds so you can watch rates evolve without restarting the crate.
- Use `--debug-first` to run only the first combo and dump every recorded error/sample summary to stdout, handy when validating new crate settings before committing to the full scan.
- On any configuration failure the harness automatically drops the crate connection, reconnects, and retries once before marking the combo as failed—no manual power cycle required.
- Each record now includes `readback`, which captures the values reported by the SAMPIC getters (sampling frequency, pulser period ticks, and the enabled-channel list) so you can confirm the crate applied the requested settings.
- The readout loop settings (`prepare_interval`, `max_loops`, `retry_sleep_us`) mirror the stand-alone pulser test defaults.
- Results are written to `scripts/tools/sampic_tests/data/deadtime_scan.jsonl` by default (override with `output.results_path`). The file is JSONL (one object per combo) and includes per-sample stats, aggregate rates, hit timestamp separation summaries, retry/error counts, and the runtime metadata.
- On restart, the tool skips previously completed combinations (records with `"status":"complete"`). Delete a line to force a rerun, or edit the config to narrow the space mid-way through a campaign.

### Lecroy double-pulse deadtime scan

The double-pulse mode in `sampic_deadtime_scan` reuses the same acquisition harness but swaps the on-board pulser for Pascal’s Lecroy 9210 generator so we can sweep the inter-pulse spacing of a double pulse while watching the externally triggered channel (currently channel 14). It expects the Lecroy trigger output to be wired into the crate’s external trigger input and the analog double pulse routed to the desired SAMPIC channel.

Build via the shared `cmake` command, then run:

```bash
scripts/tools/sampic_tests/scripts/run.sh --double-pulse -- \
  --config scripts/tools/sampic_tests/config/double_pulse_deadtime_scan.default.json

# or run the binary directly
scripts/tools/sampic_tests/build/bin/sampic_deadtime_scan --mode double-pulse \
  --config scripts/tools/sampic_tests/config/double_pulse_deadtime_scan.default.json
```

Highlights:

- `config/double_pulse_deadtime_scan.default.json` defines the digitizer-rate × double-pulse spacing grid plus a single-channel mask set to `[14]`. Adjust `scan.channels` if the cabling moves; unlike the pulser scan we keep the channel list fixed for every combo.
- The `external_trigger` block controls how the crate interprets the Lecroy TTL (type/edge/level). Defaults match Pascal’s wiring: TTL-level rising-edge external trigger on both the trigger and sync paths.
- The `lecroy` block mirrors the GUI defaults (frequency, amplitude/baseline, width, etc.) and exposes the TCP endpoint (`10.0.1.102:1234`). Edit these fields to match the currently cabled head (e.g., toggle `output_inverse`, choose channel `B`, or disable `double_pulse_enabled` if you want to debug single pulses).
- Before the scan starts, the harness connects to the generator, loads the base config (trigger mode, amplitude, outputs, double pulse toggle), and then rewrites `:<channel>:DEL` prior to each combo. The optional `settle_delay_s` and `manual_trigger` knobs let you pause or fire the generator after each update if you prefer SINGLE/BURST mode on the Lecroy.
- Each record includes the requested double-pulse spacing (`parameters.double_pulse_delay_ns`) and the static Lecroy configuration block so you can reconstruct the waveform that produced the data. Results land in `scripts/tools/sampic_tests/data/double_pulse_deadtime_scan.jsonl`.
- Like the pulser scan, the harness retries crate start/stop transitions, logs per-sample stats, and can resume by skipping completed combos. Use `--debug-first` during bring-up to watch every sample/error without running the full grid.
- Run `scripts/tools/sampic_tests/scripts/helpers/lecroy/test.sh --config <json>` to push a configuration into the generator and read back its ID (`*IDN?`) without starting the SAMPIC run. Adding `--delay-ns <value>` lets you spot-check the double-pulse spacing interactively.
- Need a no-config-file tweak? `scripts/tools/sampic_tests/scripts/helpers/lecroy/quick_set.py --frequency-hz 50 --channel A --width-ns 2.4` applies only the provided parameters and then prints a full readback so you can confirm the settings on the console.
- `--list-modes` shows the currently built-in modes (`pulser-rate`, `crate-smoke`, `deadtime`, `double-pulse`) if you want to call the binary directly.
# L2 external-trigger gate probe

`l2_external_gate_probe` is a standalone hardware test, not a MIDAS frontend mode.
It configures one FEB for self-triggered channel primitives, builds their L2 OR,
and optionally requires coincidence with the Control Board external-trigger gate.
It also enables and prints the external-trigger counter stream.

> **Warning**
>
> `--apply` changes live crate settings. In particular, enabling L2 construction
> is crate-wide in the vendor API even though this probe configures one selected
> FEB. Stop the MIDAS frontend and any other crate client before using it.

Build it with `./scripts/build.sh`, then first validate the configuration without
touching the crate:

```bash
./scripts/helpers/l2_external_gate_probe.sh \
  --config config/l2_external_gate_probe.example.json
```

Run the gated measurement only when the LPNHE wiring is confirmed (Lecroy module A
to the selected FEB input; module B, delayed by 50 ns, to the Control Board external
trigger input):

```bash
./scripts/helpers/l2_external_gate_probe.sh \
  --config config/l2_external_gate_probe.example.json --apply
```

For an ungated baseline using otherwise identical settings, add `--without-gate`.
Compare accepted hit/event rates and the printed trigger records. The external gate
is in 10 ns clock periods and the vendor library enforces a minimum of 3 (30 ns).

The probe also retains timestamps through the entire run and prints an end-of-run
hit-to-external-trigger correlation. `external_trigger.records_per_frame` is set to
one so the Control Board transmits each counter record promptly rather than its
default batch of 127. Both timestamp sources are in the same 10 ns domain at the
example 6.4 GHz sampling rate. `correlation.expected_hit_minus_trigger_ns` is the
fixed FEB-hit minus Control-Board-trigger offset; begin with zero, inspect the
reported delta, then set the measured offset and a suitable tolerance. This checks
the timing association; compare it with the `--without-gate` baseline to establish
that the gate also rejects non-coincident primitives.

### Gated versus ungated channel-rate comparison

To compare the effect of the gate without changing any other configured setting,
run:

```bash
./scripts/helpers/l2_external_gate_probe.sh \
  --config config/l2_external_gate_probe.example.json --apply --compare
```

The probe runs the ungated L2 baseline first, then an equal-duration gated run.
It reports aggregate event/hit/trigger-record rates and a channel table with the
gated-to-ungated hit-rate ratio. Because the external-trigger record rate can vary
between phases, it also reports events/hits per trigger record and a per-channel
trigger-normalized ratio; use that final ratio for the gating conclusion.
`comparison.duration_s` controls each phase. The crate is left configured in gated
mode when the comparison completes.

### Lecroy-B output gate-control test

This is the decisive hardware-gate check. It changes only the configured Lecroy
gate-output channel's `DISA` state (normally B), records gated data with B enabled,
then gated data with B disabled, and finally ungated data with B still disabled.
The original B-output state is restored even if a SAMPIC phase fails.

```bash
./scripts/helpers/l2_external_gate_probe.sh \
  --config config/l2_external_gate_probe.example.json --apply --b-output-gate-test
```

Each phase uses `comparison.duration_s`. Expected result: gated+B-disabled falls to
background, while ungated+B-disabled returns to the self-trigger rate. This mode
does not alter Lecroy frequency, amplitude, widths, delays, or module-A settings.
