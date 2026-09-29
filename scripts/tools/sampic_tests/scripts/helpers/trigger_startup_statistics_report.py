#!/usr/bin/env python3
"""Render the preallocated trigger-startup scan as a two-page PDF."""

import csv
import json
from collections import defaultdict
from pathlib import Path
from statistics import median

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Rectangle


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/trigger_startup_readiness_test/preallocated_statistics_scan"
OUTPUT = DATA / "trigger_startup_readiness_report.pdf"
NAVY = "#152238"
BLUE = "#2375a5"
TEAL = "#138a85"
ORANGE = "#d97a2b"
GRAY = "#5d6775"
PALE = "#eef3f6"


def load():
    grouped = defaultdict(list)
    with (DATA / "manifest.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            if row["status"] != "complete":
                raise ValueError(f"Incomplete trial: {row}")
            path = DATA / row["run_dir"] / "summary.json"
            with path.open() as stream:
                summary = json.load(stream)
            rate = int(row["rate_hz"])
            if int(summary["rate_hz"]) != rate:
                raise ValueError(f"Rate mismatch: {path}")
            grouped[rate].append(summary)
    if sorted(grouped) != [1000, 5000, 10000, 20000]:
        raise ValueError(f"Unexpected rates: {sorted(grouped)}")
    if any(len(grouped[rate]) != 20 for rate in grouped):
        raise ValueError("Expected 20 completed trials per rate")
    return grouped


def page(title, subtitle, number):
    fig = plt.figure(figsize=(8.27, 11.69), facecolor="white")
    fig.add_artist(Rectangle((0, 0.965), 1, 0.035, transform=fig.transFigure,
                             color=NAVY, zorder=0))
    fig.text(0.07, 0.928, title, fontsize=21, fontweight="bold", color=NAVY)
    fig.text(0.07, 0.895, subtitle, fontsize=9, color=GRAY)
    fig.add_artist(Rectangle((0.07, 0.87), 0.86, 0.002, transform=fig.transFigure,
                             color=BLUE))
    fig.text(0.07, 0.04, "SAMPIC  /  PREALLOCATED STATISTICS SCAN",
             fontsize=7.5, color=GRAY)
    fig.text(0.93, 0.04, f"{number} / 2", ha="right", fontsize=8, color=GRAY)
    return fig


def label_box(fig, x, y, width, value, label):
    fig.add_artist(Rectangle((x, y), width, 0.076, transform=fig.transFigure,
                             facecolor=PALE, edgecolor="none"))
    fig.text(x + 0.015, y + 0.039, value, fontsize=18, color=NAVY,
             fontweight="bold")
    fig.text(x + 0.015, y + 0.014, label, fontsize=8, color=GRAY)


def paragraph(fig, x, y, lines, color=NAVY, size=9.2, spacing=0.020):
    for i, line in enumerate(lines):
        fig.text(x, y - i * spacing, line, color=color, fontsize=size, va="top")


def main():
    grouped = load()
    rates = sorted(grouped)
    names = [f"{rate // 1000} kHz" for rate in rates]
    all_runs = [run for rate in rates for run in grouped[rate]]
    total_records = sum(r["trigger_records"] for r in all_runs)
    total_missing = sum(r["inferred_internal_missing"] for r in all_runs)
    ready = sum(r["receiver_ready_before_enable"] for r in all_runs)
    realloc = sum(r["timestamp_reallocation_occurred"] for r in all_runs)
    missing = [[r["inferred_internal_missing"] for r in grouped[rate]]
               for rate in rates]
    missing_sum = [sum(values) for values in missing]
    records_sum = [sum(r["trigger_records"] for r in grouped[rate])
                   for rate in rates]
    ppm = [1e6 * m / (m + n) for m, n in zip(missing_sum, records_sum)]

    with PdfPages(OUTPUT, metadata={"Title": "Trigger-startup readiness report",
                                   "Author": "SAMPIC test analysis",
                                   "Subject": "Preallocated statistics scan, 16 September 2026"}) as pdf:
        fig = page("Trigger-startup readiness", "Preallocated statistics scan  |  16 September 2026", 1)
        label_box(fig, 0.07, 0.775, 0.265, "80 / 80", "completed trials")
        label_box(fig, 0.365, 0.775, 0.265, f"{ready} / 80", "receiver ready before enable")
        label_box(fig, 0.66, 0.775, 0.265, str(total_missing), "inferred internal gaps")
        fig.text(0.07, 0.727, "Key finding", fontsize=12, fontweight="bold", color=NAVY)
        paragraph(fig, 0.07, 0.704, [
            "The receiver entered its read loop before Lecroy enable in every trial; no",
            "timestamp-vector reallocation occurred. Small internal timestamp gaps remain.",
            "These data do not establish whether the first post-enable trigger was captured."
        ])

        ax = fig.add_axes((0.12, 0.405, 0.79, 0.205))
        bars = ax.bar(names, ppm, color=[TEAL, TEAL, BLUE, ORANGE], width=0.60)
        ax.set_ylabel("Inferred missing per million records", fontsize=9)
        ax.set_ylim(0, max(ppm) * 1.24)
        ax.spines[["top", "right"]].set_visible(False)
        ax.grid(axis="y", alpha=0.18)
        ax.set_axisbelow(True)
        for bar, value in zip(bars, ppm):
            ax.text(bar.get_x() + bar.get_width() / 2, value + 2,
                    f"{value:.0f}", ha="center", fontsize=9, color=NAVY)
        fig.text(0.07, 0.632, "Gap rate by programmed trigger frequency",
                 fontsize=12, fontweight="bold", color=NAVY)

        fig.text(0.07, 0.356, "Rate", fontsize=9, fontweight="bold", color=NAVY)
        fig.text(0.29, 0.356, "Records", fontsize=9, fontweight="bold", color=NAVY)
        fig.text(0.53, 0.356, "Gaps", fontsize=9, fontweight="bold", color=NAVY)
        fig.text(0.72, 0.356, "Trials with gaps", fontsize=9, fontweight="bold", color=NAVY)
        for i, rate in enumerate(rates):
            y = 0.327 - i * 0.036
            if i % 2 == 0:
                fig.add_artist(Rectangle((0.07, y - 0.008), 0.86, 0.033,
                                         transform=fig.transFigure, color=PALE))
            for x, value in [(0.07, names[i]), (0.29, f"{records_sum[i]:,}"),
                             (0.53, str(missing_sum[i])),
                             (0.72, f"{sum(v > 0 for v in missing[i])} / 20")]:
                fig.text(x, y, value, fontsize=9, color=NAVY)
        fig.text(0.07, 0.147, f"Total captured: {total_records:,} records. "
                 f"No reallocations: {realloc} / 80 trials.", fontsize=8.7, color=GRAY)
        fig.text(0.07, 0.123, "Gap rate = inferred internal missing / (records + inferred missing).",
                 fontsize=8.2, color=GRAY)
        fig.text(0.07, 0.099, "Source: manifest.csv and all 80 per-trial summary.json files.",
                 fontsize=8.2, color=GRAY)
        pdf.savefig(fig)
        plt.close(fig)

        fig = page("Distribution and interpretation", "20 five-second trials at each programmed rate", 2)
        ax = fig.add_axes((0.12, 0.626, 0.79, 0.202))
        ax.boxplot(missing, labels=names, patch_artist=True, showfliers=True,
                   boxprops={"facecolor": "#cfe5e6", "edgecolor": TEAL},
                   medianprops={"color": NAVY, "linewidth": 1.6},
                   whiskerprops={"color": TEAL}, capprops={"color": TEAL},
                   flierprops={"markerfacecolor": ORANGE, "markeredgecolor": ORANGE,
                               "markersize": 4})
        ax.set_ylabel("Inferred missing per trial", fontsize=9)
        ax.spines[["top", "right"]].set_visible(False)
        ax.grid(axis="y", alpha=0.18)
        ax.set_axisbelow(True)
        fig.text(0.07, 0.840, "Internal gaps vary between trials",
                 fontsize=12, fontweight="bold", color=NAVY)
        fig.text(0.07, 0.589, "At 20 kHz, the median was 4.5 missing records per trial; the largest",
                 fontsize=9, color=NAVY)
        fig.text(0.07, 0.569, "single count was 32 (trial 012), followed by 29 (trial 001).",
                 fontsize=9, color=NAVY)

        fig.text(0.07, 0.521, "Readiness timing", fontsize=12, fontweight="bold", color=NAVY)
        leads = [[r["lecroy_enable_started_us"] - r["first_read_started_us"]
                  for r in grouped[rate]] for rate in rates]
        fig.text(0.07, 0.494, "Median lead before enable:", fontsize=9, color=NAVY)
        fig.text(0.40, 0.494,
                 "  |  ".join(f"{name}: {median(values):.1f} µs"
                             for name, values in zip(names, leads)),
                 fontsize=8.2, color=BLUE)
        fig.text(0.07, 0.472, f"Shortest observed lead: {min(map(min, leads)):.1f} µs. "
                 "Ready means first read call began, not first event received.",
                 fontsize=8.6, color=GRAY)

        fig.text(0.07, 0.415, "What this scan can and cannot say", fontsize=12,
                 fontweight="bold", color=NAVY)
        paragraph(fig, 0.07, 0.386, [
            "• The gap algorithm tests intervals between recorded timestamps against",
            "  an expected period of 1 / (3 × programmed rate), with a tolerance.",
            "• It cannot count lost triggers before the first or after the last record,",
            "  or irregular gaps outside that tolerance.",
            "• There is no shared absolute time for the first generator edge and first",
            "  captured record. Startup-only loss is therefore not measurable here.",
            "• Counts arrive in complete 127-trigger events. Capture counts exceeding",
            "  rate × 5 s are not evidence of greater-than-100% efficiency."
        ], size=8.8, spacing=0.027)
        fig.text(0.07, 0.135, "Recommended next measurement", fontsize=10,
                 fontweight="bold", color=NAVY)
        paragraph(fig, 0.07, 0.110, [
            "Log a common reference for the first emitted edge and first captured trigger,",
            "or use an independent generator pulse counter; retain failed-attempt logs."
        ], size=8.2, spacing=0.019)
        pdf.savefig(fig)
        plt.close(fig)

    print(OUTPUT)


if __name__ == "__main__":
    main()
