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
- Run `scripts/tools/sampic_tests/scripts/lecroy/test.sh --config <json>` to push a configuration into the generator and read back its ID (`*IDN?`) without starting the SAMPIC run. Adding `--delay-ns <value>` lets you spot-check the double-pulse spacing interactively.
- `--list-modes` shows the currently built-in modes (`pulser-rate`, `crate-smoke`, `deadtime`, `double-pulse`) if you want to call the binary directly.
