#!/usr/bin/env python3
"""Sweep source rates and measure maximum sustained full-chain throughput."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys


VARIANTS = (("uncut", False), ("punch-cut", True))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rates", default="250,500,1000,2000")
    parser.add_argument("--duration", type=float, default=3.0)
    parser.add_argument("--crates", type=int, default=2)
    parser.add_argument("--hits-per-event", type=int, default=8)
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--midas-root", type=Path, default=Path("/home/jack/software/midas"))
    parser.add_argument("--append", action="store_true",
                        help="merge these rates into the canonical existing sweep")
    args = parser.parse_args()

    rates = [float(value) for value in args.rates.split(",")]
    benchmark = Path(__file__).with_name("run_midas_benchmark.py")
    project = benchmark.resolve().parents[4]
    current = project / ".artifacts/midas-streaming-benchmark/current/results.json"
    output = project / ".artifacts/midas-streaming-benchmark/saturation"
    output.mkdir(parents=True, exist_ok=True)
    destination = output / "results.json"
    if args.append and destination.exists():
        results = json.loads(destination.read_text(encoding="utf-8"))["runs"]
    else:
        results = {name: [] for name, _ in VARIANTS}

    for name, select in VARIANTS:
        for rate in rates:
            command = [
                sys.executable, str(benchmark), "--crates", str(args.crates),
                "--duration", str(args.duration), "--rate-hz", str(rate),
                "--hits-per-event", str(args.hits_per_event),
                "--samples", str(args.samples), "--midas-root", str(args.midas_root),

            ]
            if select: command.append("--select")
            print(f"Running {name} at {rate:g} Hz/crate...", flush=True)
            completed = subprocess.run(command, check=True, text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            result = json.loads(current.read_text(encoding="utf-8"))
            result["requested_rate_per_crate_hz"] = rate
            results[name] = [existing for existing in results[name]
                             if existing["requested_rate_per_crate_hz"] != rate]
            results[name].append(result)
            results[name].sort(key=lambda run: run["requested_rate_per_crate_hz"])

    summary = {}
    for name, runs in results.items():
        best = max(runs, key=lambda run: run["steady_state_input_hits_per_second"])
        summary[name] = {
            "maximum_sustained_hits_per_second": best["steady_state_input_hits_per_second"],
            "maximum_sustained_raw_bank_megabytes_per_second":
                best["steady_state_input_bank_megabytes_per_second"],
            "published_megabytes_per_second_at_maximum":
                best["steady_state_output_megabytes_per_second"],
            "input_output_byte_ratio_at_maximum":
                best["input_output_byte_ratio"],
            "requested_rate_per_crate_hz": best["requested_rate_per_crate_hz"],
            "selection_rejection_fraction": best["selection_rejection_fraction"],
        }
    document = {"runs": results, "summary": summary}
    destination.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# SAMPIC streaming saturation benchmark", "",
        "Compare no cuts with the optional low-amplitude all-plane punch-through cut.", "",
        "| Mode | Maximum input hits/s | Raw-bank MB/s | Published MB/s | "
        "Raw-bank/product ratio | Rejected events |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for name, values in summary.items():
        lines.append(
            f"| {name} | {values['maximum_sustained_hits_per_second']:.0f} | "
            f"{values['maximum_sustained_raw_bank_megabytes_per_second']:.2f} | "
            f"{values['published_megabytes_per_second_at_maximum']:.2f} | "
            f"{values['input_output_byte_ratio_at_maximum']:.2f}x | "
            f"{100 * values['selection_rejection_fraction']:.2f}% |")
    lines.extend([
        "", "Throughput uses live pre-STOP counters and excludes buffered "
        "end-of-run flushes. Ratios use exact post-STOP byte totals.", "",
    ])
    (output / "report.md").write_text("\n".join(lines), encoding="utf-8")
    subprocess.run([
        sys.executable, str(Path(__file__).with_name("generate_saturation_report.py")),
        str(destination), "--output", str(output / "report.pdf")
    ], check=True)
    print(destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
