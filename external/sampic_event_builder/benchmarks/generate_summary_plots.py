#!/usr/bin/env python3
"""Generate the two presentation-ready SAMPIC benchmark summary plots."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def add_value_labels(axis, bars, formatter) -> None:
    for bar in bars:
        axis.annotate(
            formatter(bar.get_height()),
            (bar.get_x() + bar.get_width() / 2, bar.get_height()),
            xytext=(0, 4), textcoords="offset points",
            ha="center", va="bottom", fontsize=9)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--output-directory", type=Path)
    args = parser.parse_args()

    document = json.loads(args.results.read_text(encoding="utf-8"))
    runs = document["runs"]
    summary = document["summary"]
    output = args.output_directory or args.results.parent
    output.mkdir(parents=True, exist_ok=True)

    scaling = json.loads(
        args.results.with_name("raw_bank_scaling.json").read_text(encoding="utf-8"))
    stages = ["Event building", "Selection", "Fitting", "Compression", "Finalization"]
    capacities = [
        max(point["event_building_service_hits_per_second"]
            for point in scaling["measurements"]),
        max(run["stages"]["Selection"]["service_hits_per_second"]
            for run in runs["raw"]),
        max(run["stages"]["Fitting"]["service_hits_per_second"]
            for run in runs["fit-compress"]),
        max(run["stages"]["Compression"]["service_hits_per_second"]
            for run in runs["fit-compress"]),
        max(run["stages"]["Finalization"]["service_hits_per_second"]
            for run in runs["fit-compress"]),
    ]

    fig, axis = plt.subplots(figsize=(10, 6))
    bars = axis.bar(stages, capacities, color="tab:blue")
    axis.set_title("Isolated stage capacity")
    axis.set_ylabel("Service capacity (hits/s)")
    axis.set_yscale("log")
    axis.grid(axis="y", alpha=0.25)
    axis.tick_params(axis="x", rotation=20)
    add_value_labels(
        axis, bars,
        lambda value: f"{value / 1e6:.1f}M" if value >= 1e6
        else f"{value / 1e3:.1f}k")
    fig.tight_layout()
    fig.savefig(output / "isolated_stage_capacity.png", dpi=200)
    plt.close(fig)

    configurations = [
        "Null operation",
        "Selection only",
        "Selection + fit + compression",
    ]
    ratios = [
        1.0,
        summary["raw"]["full_chain_compression_ratio_at_maximum"],
        summary["fit-compress"]["full_chain_compression_ratio_at_maximum"],
    ]
    fig, axis = plt.subplots(figsize=(10, 6))
    bars = axis.bar(configurations, ratios, color="tab:blue")
    axis.axhline(1.0, color="black", linewidth=1)
    axis.set_title("Whole-chain data reduction")
    axis.set_ylabel("Incoming raw bytes / published product bytes")
    axis.set_ylim(0, max(ratios) * 1.18)
    axis.grid(axis="y", alpha=0.25)
    add_value_labels(axis, bars, lambda value: f"{value:.2f}x")
    fig.tight_layout()
    fig.savefig(output / "whole_chain_data_reduction.png", dpi=200)
    plt.close(fig)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
