#!/usr/bin/env python3
"""Plot stage capacity, full-chain saturation, compression, and selection."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np


def label_bars(ax, bars, format_string="{:,.0f}"):
    for bar in bars:
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                format_string.format(bar.get_height()), ha="center", va="bottom",
                fontsize=8, rotation=0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    document = json.loads(args.results.read_text(encoding="utf-8"))
    runs = document["runs"]
    summary = document["summary"]
    output = args.output or args.results.with_name("report.pdf")
    variants = list(summary)
    scaling_path = args.results.with_name("raw_bank_scaling.json")
    raw_bank_scaling = (
        json.loads(scaling_path.read_text(encoding="utf-8"))["measurements"]
        if scaling_path.is_file() else []
    )

    # Use the operational fit+compression path for processing stages, while
    # event building and selection use the largest clean raw-path measurement.
    active_runs = runs["fit-compress"]
    raw_runs = runs["raw"]
    stage_sources = {
        "Event building": raw_runs,
        "Selection": raw_runs,
        "Fitting": active_runs,
        "Compression": active_runs,
        "Finalization": active_runs,
    }
    stage_names = list(stage_sources)
    stage_hits = [max(run["stages"][stage]["service_hits_per_second"]
                      for run in stage_sources[stage]) for stage in stage_names]
    stage_mb = [max(run["stages"][stage]["service_input_megabytes_per_second"]
                    for run in stage_sources[stage]) for stage in stage_names]
    if raw_bank_scaling:
        # The original sweep predates removal of the builder's redundant
        # whole-pending-buffer sort and copies. Replace only that obsolete
        # isolated-stage value with the post-optimization measurement.
        stage_hits[stage_names.index("Event building")] = max(
            point["event_building_service_hits_per_second"]
            for point in raw_bank_scaling)
        stage_mb[stage_names.index("Event building")] = max(
            point["event_building_service_megabytes_per_second"]
            for point in raw_bank_scaling)

    with PdfPages(output) as pdf:
        fig = plt.figure(figsize=(11, 8.5))
        fig.text(0.08, 0.92, "SAMPIC streaming throughput and positron filtering",
                 fontsize=19, weight="bold")
        best_fit = summary["fit-compress"]
        text = (
            f"Maximum fit+compression chain: "
            f"{best_fit['maximum_sustained_hits_per_second']:,.0f} input hits/s, "
            f"{best_fit['maximum_sustained_raw_bank_megabytes_per_second']:.2f} raw-bank MB/s\n"
            f"Punch-through rejection at that point: "
            f"{100 * best_fit['selection_rejection_fraction']:.1f}% (simulated target: 75%)\n"
            f"Full-chain raw-bank/product ratio: "
            f"{best_fit['full_chain_compression_ratio_at_maximum']:.2f}x\n\n"
            "Raw-chain pacing follow-up: 507,492 live hits/s (87.34 MB/s) with 128 hits/bank.\n"
            "The earlier 140 kHit/s raw result was limited by per-event simulator sleeps.\n\n"
            "Simulated punch-through events contain low-amplitude hits spanning all eight ATAR planes.\n"
            "The selection stage rejects those candidates before optional fitting and compression.\n"
            "Stage capacity is measured from time inside each stage; chain capacity includes MIDAS I/O.\n"
            "Compression ratios compare complete incoming SR bank payloads with complete outgoing SE products."
        )
        fig.text(0.08, 0.75, text, fontsize=12, va="top", linespacing=1.6)
        plt.axis("off"); pdf.savefig(fig); plt.close(fig)

        fig, axes = plt.subplots(1, 2, figsize=(13, 6))
        stage_colors = ["tab:green" if name == "Event building" else "tab:blue"
                        for name in stage_names]
        bars = axes[0].bar(stage_names, stage_hits, color=stage_colors)
        axes[0].set(title="Current isolated stage service capacity",
                    ylabel="hits/s", yscale="log")
        axes[0].tick_params(axis="x", rotation=25); label_bars(axes[0], bars)
        bars = axes[1].bar(stage_names, stage_mb, color=stage_colors)
        axes[1].set(title="Current isolated stage input bandwidth",
                    ylabel="MB/s", yscale="log")
        axes[1].tick_params(axis="x", rotation=25); label_bars(axes[1], bars, "{:.1f}")
        for ax in axes:
            ax.grid(axis="y", alpha=0.25)
            ax.text(0.02, 0.98,
                    "Green: post-optimization builder\nBlue: stage timings from original sweep",
                    transform=ax.transAxes, va="top", fontsize=8,
                    bbox={"facecolor": "white", "alpha": 0.85, "edgecolor": "0.8"})
        fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        if raw_bank_scaling:
            bank_sizes = [point["hits_per_bank"] for point in raw_bank_scaling]
            live_hits = [point["live_hits_per_second"] / 1.0e3
                         for point in raw_bank_scaling]
            bandwidth = [point["raw_bank_megabytes_per_second"]
                         for point in raw_bank_scaling]
            builder_capacity = [
                point["event_building_service_hits_per_second"] / 1.0e6
                for point in raw_bank_scaling]

            fig, axes = plt.subplots(1, 2, figsize=(13, 5.5))
            axes[0].plot(bank_sizes, live_hits, "o-", linewidth=2,
                         label="complete raw chain")
            for x, y in zip(bank_sizes, live_hits):
                axes[0].annotate(f"{y:.0f} kHit/s", (x, y),
                                 xytext=(0, 8), textcoords="offset points",
                                 ha="center")
            axes[0].set(
                title="Raw-chain throughput vs. MIDAS bank size",
                xlabel="hits per raw MIDAS bank",
                ylabel="live input throughput (kHit/s)",
                xscale="log", xticks=bank_sizes,
                ylim=(0, max(live_hits) * 1.18))
            axes[0].get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
            axes[0].grid(alpha=0.25)

            axes[1].plot(bank_sizes, bandwidth, "o-", linewidth=2,
                         color="tab:orange", label="raw-bank input")
            for x, y in zip(bank_sizes, bandwidth):
                axes[1].annotate(f"{y:.1f} MB/s", (x, y),
                                 xytext=(0, 8), textcoords="offset points",
                                 ha="center")
            axes[1].set(
                title="Raw-bank input bandwidth",
                xlabel="hits per raw MIDAS bank", ylabel="MB/s",
                xscale="log", xticks=bank_sizes,
                ylim=(0, max(bandwidth) * 1.18))
            axes[1].get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
            axes[1].grid(alpha=0.25)
            fig.suptitle(
                "Corrected source pacing: per-bank overhead matters below ~32 hits",
                fontsize=15, weight="bold")
            fig.text(
                0.5, 0.015,
                "The isolated event-building stage measured "
                f"{min(builder_capacity):.1f}–{max(builder_capacity):.1f} MHit/s; "
                "the ~0.5 MHit/s plateau belongs to the complete MIDAS-to-product chain.",
                ha="center", fontsize=10)
            fig.tight_layout(rect=(0, 0.05, 1, 0.94))
            pdf.savefig(fig); plt.close(fig)

        max_hits = [summary[name]["maximum_sustained_hits_per_second"] for name in variants]
        max_mb = [summary[name]["maximum_sustained_raw_bank_megabytes_per_second"]
                  for name in variants]
        fig, axes = plt.subplots(1, 2, figsize=(12, 5.5))
        bars = axes[0].bar(variants, max_hits)
        axes[0].set(title="Original per-event-pacing sweep", ylabel="input hits/s")
        label_bars(axes[0], bars)
        bars = axes[1].bar(variants, max_mb)
        axes[1].set(title="Raw MIDAS-bank rate at maximum", ylabel="input MB/s")
        label_bars(axes[1], bars, "{:.2f}")
        for ax in axes: ax.grid(axis="y", alpha=0.25)
        fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        ratios = [summary[name]["full_chain_compression_ratio_at_maximum"]
                  for name in variants]
        fig, ax = plt.subplots(figsize=(10, 6))
        bars = ax.bar(variants, ratios)
        ax.axhline(1.0, color="black", linewidth=1)
        ax.set(title="Whole-chain data reduction relative to raw MIDAS banks",
               ylabel="incoming SR bytes / outgoing SE bytes")
        label_bars(ax, bars, "{:.2f}x"); ax.grid(axis="y", alpha=0.25)
        fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        fig, axes = plt.subplots(1, 2, figsize=(13, 5.5))
        all_requested = []
        all_sustained = []
        all_rejection = []
        for name in variants:
            requested_hits = [run["requested_rate_per_crate_hz"] *
                              run["configuration"]["crates"] *
                              run["configuration"]["hits_per_event"]
                              for run in runs[name]]
            sustained = [run["steady_state_input_hits_per_second"] for run in runs[name]]
            rejection = [100.0 * run["selection_rejection_fraction"] for run in runs[name]]
            all_requested.extend(requested_hits)
            all_sustained.extend(sustained)
            all_rejection.extend(rejection)
            axes[0].plot(requested_hits, sustained, "o-", label=name)
            axes[1].plot(requested_hits, rejection, "o-", label=name)
        ideal_limit = max(all_sustained, default=1.0) * 1.10
        axes[0].plot([0, ideal_limit], [0, ideal_limit], "k--", alpha=0.4,
                     label="requested = sustained")
        axes[0].set(title="Original pacing sweep (superseded for raw)", xlabel="requested hits/s",
                    ylabel="sustained input hits/s", xscale="log",
                    ylim=(0, ideal_limit))
        axes[1].axhline(75.0, color="black", linestyle="--", label="75% target")
        rejection_padding = 0.5
        rejection_min = min(all_rejection, default=75.0)
        rejection_max = max(all_rejection, default=75.0)
        axes[1].set(title="Punch-through rejection stability", xlabel="requested hits/s",
                    ylabel="events rejected (%)", xscale="log",
                    ylim=(rejection_min - rejection_padding,
                          rejection_max + rejection_padding))
        for ax in axes: ax.legend(); ax.grid(alpha=0.25)
        fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
