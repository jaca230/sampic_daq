# sampic_rate_tests

A lightweight playground for SAMPIC crate experimentation outside the MIDAS frontend.  The initial utility, `sampic_pulser_rate`, connects directly to the crate, enables the on-board pulser, and measures the resulting acquisition rate.

The code is a C++ port of the vendor `sampic_test.c` example with additional pulser-specific configuration and timing diagnostics.  Use this tool to iterate on high-rate settings before shipping changes into the MIDAS frontend.

## Build

```bash
cmake -S scripts/tools/sampic_rate_tests -B scripts/tools/sampic_rate_tests/build
cmake --build scripts/tools/sampic_rate_tests/build
```

This project reuses the main build’s configuration for the vendor driver, so run it from the repository root.

## Usage

```bash
scripts/tools/sampic_rate_tests/build/bin/sampic_pulser_rate \
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
