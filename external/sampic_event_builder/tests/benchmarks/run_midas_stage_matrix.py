#!/usr/bin/env python3
"""Compare the uncut and optional punch-through-cut MIDAS paths."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys


VARIANTS = (("uncut", False), ("punch-cut", True))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crates", type=int, default=2)
    parser.add_argument("--duration", type=float, default=10.0)
    parser.add_argument("--rate-hz", type=float, default=1000.0)
    parser.add_argument("--hits-per-event", type=int, default=16)
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--window-ns", type=int, default=200)
    parser.add_argument("--midas-root", type=Path, default=Path("/home/jack/software/midas"))
    args = parser.parse_args()

    benchmark = Path(__file__).with_name("run_midas_benchmark.py")
    project = benchmark.resolve().parents[4]
    benchmark_root = project / ".artifacts/midas-streaming-benchmark"
    current_result = benchmark_root / "current/results.json"
    results: dict[str, dict] = {}

    for name, select in VARIANTS:
        command = [
            sys.executable, str(benchmark),
            "--crates", str(args.crates),
            "--duration", str(args.duration),
            "--rate-hz", str(args.rate_hz),
            "--hits-per-event", str(args.hits_per_event),
            "--samples", str(args.samples),
            "--window-ns", str(args.window_ns),
            "--midas-root", str(args.midas_root),
        ]
        if select:
            command.append("--select")
        print(f"Running {name}...", flush=True)
        subprocess.run(command, check=True)
        results[name] = json.loads(current_result.read_text(encoding="utf-8"))

    output = benchmark_root / "stage-matrix"
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.json").write_text(
        json.dumps(results, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# SAMPIC MIDAS stage matrix", "",
        f"Two-process path: `{args.crates}` crate frontend(s) -> MIDAS SBUFnn buffers "
        "-> SAMPIC event builder -> MIDAS SYSTEM buffer.", "",
        f"Requested `{args.rate_hz:.1f}` vendor events/s/crate for "
        f"`{args.duration:.1f}` s with `{args.hits_per_event}` hits/event and "
        f"`{args.samples}` raw samples/hit.", "",

        "| Variant | Input hits/s | Built events/s | Product MB/s | Output bytes |",
        "|---|---:|---:|---:|---:|",
    ]
    for name, _ in VARIANTS:
        result = results[name]
        lines.append(
            f"| {name} | {result['input_hits_per_second']:.1f} | "
            f"{result['output_events_per_second']:.1f} | "
            f"{result['output_megabytes_per_second']:.3f} | "
            f"{result['output_bytes']} |")
    lines.extend([
        "", "Counts come from post-STOP producer statistics and builder-owned "
        "ODB counters, after held time windows have been flushed.", "",
    ])
    report = output / "report.md"
    report.write_text("\n".join(lines), encoding="utf-8")
    subprocess.run([
        sys.executable, str(Path(__file__).with_name("generate_midas_report.py")),
        str(output / "results.json"), "--output", str(output / "report.pdf")
    ], check=True)
    print(f"Matrix report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
