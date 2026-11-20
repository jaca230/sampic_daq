#!/usr/bin/env python3
"""Run repeatable crate-scaling benchmarks and write one canonical report."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import platform
import re
import subprocess


STAGE = re.compile(r"^(\S(?:.*?\S)?)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)$")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--crates", default="1,2,4", help="comma-separated crate counts")
    parser.add_argument("--hits", type=int, default=50_000, help="hits generated per crate")
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--window-ns", type=int, default=200)
    parser.add_argument("--build-dir", type=pathlib.Path)
    args = parser.parse_args()

    source = pathlib.Path(__file__).resolve().parents[1]
    build = args.build_dir or source / "build"
    executable = build / "sampic_event_builder_benchmark"
    if not executable.exists():
        raise SystemExit(f"benchmark not built: run {source / 'scripts/build.sh'}")

    rows: list[tuple[int, str, float, float, float]] = []
    summaries: list[str] = []
    for crates in (int(value) for value in args.crates.split(",")):
        command = [str(executable), "--crates", str(crates), "--hits", str(args.hits),
                   "--samples", str(args.samples), "--window-ns", str(args.window_ns)]
        output = subprocess.check_output(command, text=True)
        summaries.extend(line for line in output.splitlines() if line.startswith(("crates=", "selector_", "fit=", "raw_bytes=")))
        for line in output.splitlines():
            match = STAGE.match(line.strip())
            if match and match.group(1) not in {"stage"}:
                rows.append((crates, match.group(1), *(float(match.group(i)) for i in range(2, 5))))

    output_dir = source / ".artifacts" / "rate-tests" / "latest"
    output_dir.mkdir(parents=True, exist_ok=True)
    report = output_dir / "report.md"
    lines = [
        "# SAMPIC event-builder rate sweep", "",
        f"Generated: {dt.datetime.now().astimezone().isoformat(timespec='seconds')}", "",
        f"Host: `{platform.platform()}`", "",
        f"Configuration: `{args.hits}` hits/crate, `{args.samples}` samples/hit, "
        f"`{args.window_ns} ns` event window.", "", "## Stage throughput", "",
        "| Crates | Stage | Items/s | MB/s | Elapsed ms |",
        "|---:|---|---:|---:|---:|",
    ]
    lines.extend(f"| {crates} | {stage} | {items:.1f} | {mb:.1f} | {elapsed:.1f} |"
                 for crates, stage, items, mb, elapsed in rows)
    lines.extend(["", "## Selection and waveform-processing summaries", "", "```text", *summaries, "```", "",
                  "The processing row includes exact reconstruction and sample-by-sample verification.", ""])
    report.write_text("\n".join(lines))
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
