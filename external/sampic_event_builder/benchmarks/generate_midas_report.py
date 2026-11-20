#!/usr/bin/env python3
"""Generate the plotted PDF for a completed MIDAS stage matrix."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    results = json.loads(args.results.read_text(encoding="utf-8"))
    output = args.output or args.results.with_name("report.pdf")
    variants = list(results)
    stage_names = list(next(iter(results.values()))["stages"])
    x = np.arange(len(stage_names))
    width = 0.8 / len(variants)

    with PdfPages(output) as pdf:
        fit_compress = results.get("fit-compress", next(iter(results.values())))
        fitting = fit_compress["stages"].get("Fitting", {})
        ratio = (fit_compress["input_hits"] * fit_compress["configuration"]["samples"] * 2 /
                 fit_compress["output_bytes"])
        fig = plt.figure(figsize=(11, 8.5))
        fig.text(0.08, 0.92, "SAMPIC MIDAS event-builder benchmark",
                 fontsize=20, weight="bold")
        fig.text(0.08, 0.85,
                 f"{fit_compress['configuration']['crates']} simulated raw-ADC crates; "
                 f"{fit_compress['configuration']['hits_per_event']} hits/event; "
                 f"{fit_compress['configuration']['samples']} samples/hit",
                 fontsize=12)
        summary = (
            f"Fit + compression observed rate: {fit_compress['input_hits_per_second']:,.0f} hits/s\n"
            f"Fit stage service capacity: {fitting.get('service_hits_per_second', 0):,.0f} hits/s\n"
            f"Fit stage input capacity: {fitting.get('service_input_megabytes_per_second', 0):.2f} MB/s\n"
            f"Fit-quality fallback: {100 * fit_compress.get('rejected_fit_fraction', 0):.1f}% of waveforms\n"
            f"End-product compression ratio: {ratio:.2f}x (raw sample bytes / complete product bytes)\n\n"
            "The fit solves amplitude and pedestal analytically at every sub-sample time shift.\n"
            "Waveforms above the configured reduced-chi-squared threshold retain raw int16 samples.\n"
            "Direct Rice-Golomb on uncentered ADC values expands the data; centered fit residuals do not.\n"
            "Per-waveform headers and raw fallbacks are included in complete product sizes."
        )
        fig.text(0.08, 0.68, summary, fontsize=12, linespacing=1.6, va="top")
        fig.text(0.08, 0.08,
                 "Observed rates use acquisition wall time. Service capacity uses measured CPU time "
                 "inside each stage.", fontsize=10)
        plt.axis("off"); pdf.savefig(fig); plt.close(fig)

        fig, ax = plt.subplots(figsize=(11, 7))
        for index, variant in enumerate(variants):
            values = [results[variant]["stages"][stage]["observed_hits_per_second"]
                      for stage in stage_names]
            ax.bar(x + (index - (len(variants) - 1) / 2) * width,
                   values, width, label=variant)
        ax.set(title="Observed full-chain rate at every stage",
               ylabel="hits/s", xticks=x, xticklabels=stage_names)
        ax.legend(); ax.grid(axis="y", alpha=0.25); fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        fig, ax = plt.subplots(figsize=(11, 7))
        for index, variant in enumerate(variants):
            values = [results[variant]["stages"][stage]["service_hits_per_second"]
                      for stage in stage_names]
            ax.bar(x + (index - (len(variants) - 1) / 2) * width,
                   values, width, label=variant)
        ax.set(title="Measured stage service capacity", ylabel="hits/s (log scale)",
               yscale="log", xticks=x, xticklabels=stage_names)
        ax.legend(); ax.grid(axis="y", alpha=0.25); fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        for variant in variants:
            stages = results[variant]["stages"]
            input_rates = [stages[name]["observed_input_megabytes_per_second"] for name in stage_names]
            output_rates = [stages[name]["observed_output_megabytes_per_second"] for name in stage_names]
            fig, ax = plt.subplots(figsize=(11, 7))
            ax.plot(stage_names, input_rates, "o-", label="stage input")
            ax.plot(stage_names, output_rates, "s-", label="stage output")
            ax.set(title=f"{variant}: data rate through each representation",
                   ylabel="MB/s", xlabel="pipeline stage")
            ax.legend(); ax.grid(alpha=0.25); fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

        raw_bytes = np.array([results[name]["input_hits"] *
                              results[name]["configuration"]["samples"] * 2
                              for name in variants], dtype=float)
        product_bytes = np.array([results[name]["output_bytes"] for name in variants], dtype=float)
        fig, axes = plt.subplots(1, 3, figsize=(14, 6))
        axes[0].bar(variants, product_bytes / 1.0e6)
        axes[0].set(title="Published product size", ylabel="MB")
        axes[1].bar(variants, raw_bytes / product_bytes)
        axes[1].axhline(1.0, color="black", linewidth=1)
        axes[1].set(title="Raw waveform bytes / product bytes", ylabel="compression ratio")
        axes[2].bar(variants, [100.0 * results[name].get("rejected_fit_fraction", 0.0)
                               for name in variants])
        axes[2].set(title="Fit-quality fallback", ylabel="rejected waveforms (%)")
        for ax in axes: ax.grid(axis="y", alpha=0.25)
        fig.tight_layout(); pdf.savefig(fig); plt.close(fig)

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
