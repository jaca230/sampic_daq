"""Human-reviewed threshold response plots; no automatic threshold choice."""

from pathlib import Path
import csv
import json
import math


def plot(directory):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages
    import numpy as np

    directory = Path(directory)
    config = json.loads((directory / "config.json").read_text())
    metadata = json.loads((directory / "metadata.json").read_text())
    with (directory / "results.csv").open() as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError("No measured/skipped points available to plot")
    keys = sorted({(int(r["feb"]), int(r["channel"])) for r in rows})
    thresholds = sorted({float(r["threshold_v"]) for r in rows})
    values = np.full((len(keys), len(thresholds)), np.nan)
    status_counts = {}
    for r in rows:
        status_counts[r["status"]] = status_counts.get(r["status"], 0) + 1
        if r["status"] == "complete":
            values[
                keys.index((int(r["feb"]), int(r["channel"]))),
                thresholds.index(float(r["threshold_v"])),
            ] = float(r["observed_rate_hz"])
    output = directory / "plots"
    output.mkdir(exist_ok=True)
    label = "SIMULATED DATA — " if metadata.get("simulated") else ""
    label += f"{config.get('name', config['crate']['ip'])} ({config['crate']['ip']}:{config['crate']['port']}) — "
    report = output / "threshold_response.pdf"
    with PdfPages(report) as pdf:
        fig = plt.figure(figsize=(11.7, 8.3))
        fig.text(
            0.07,
            0.9,
            label.rstrip(" — ") + "\nThreshold response scan",
            fontsize=20,
            weight="bold",
        )
        text = (
            f'Run status: {metadata["status"]}\nChannels represented: {len(keys)}; threshold points: {len(thresholds)}\n'
            f'Edge: {config["edge"]}; baseline reference: {config["baseline_v"]:.4f} V\n'
            f'Acquisition: {config["duration_s"]:g} s/group; initial group = all selected channels\n'
            f'Traffic cap: {config["aggregate_rate_limit_hz"]:,.0f} aggregate recorded hits/s\n'
            f"Point statuses: {status_counts}\n\n"
            "How to read this report\n"
            "• Curves show recorded self-trigger hits/s, not the true discriminator-crossing rate.\n"
            "• Shared readout, ADC dead time and transport losses can reduce observed rates.\n"
            "• Adaptive grouping separates overloaded groups; group size is retained in results.csv.\n"
            "• Complete zero-hit samples are valid observations; they are not proof of inactivity.\n"
            "  Their Poisson 95% rate upper bound is about 3/live_time in results.csv.\n"
            "• Triangles mark partial, guard-limited measurements, not a saturation plateau.\n"
            "• Bottom crosses mark failed or unmeasured points, not zero rates.\n"
            "• Gray heatmap cells were not measured successfully.\n\n"
            "No common good-rate assumption or automatic operating threshold is applied.\n"
            "The operator chooses thresholds using each channel’s curve and measurement context.\n"
            "Existing generator outputs were not changed. Hardware configuration was diagnostic;\n"
            "reapply the frontend ODB configuration before resuming normal acquisition."
        )
        fig.text(0.07, 0.79, text, fontsize=11, va="top", linespacing=1.5)
        pdf.savefig(fig)
        plt.close(fig)

        fig, ax = plt.subplots(figsize=(12, max(6, min(16, len(keys) * 0.09))))
        cmap = plt.get_cmap("viridis").copy()
        cmap.set_bad("#d0d0d0")
        image = ax.imshow(
            np.ma.masked_invalid(np.log10(1 + values)),
            aspect="auto",
            interpolation="nearest",
            cmap=cmap,
        )
        stride = max(1, len(keys) // 32)
        ax.set_yticks(
            range(0, len(keys), stride), [f"{f}:{c}" for f, c in keys[::stride]]
        )
        positions = sorted(
            set(
                [
                    0,
                    len(thresholds) - 1,
                    *range(0, len(thresholds), max(1, len(thresholds) // 8)),
                ]
            )
        )
        ax.set_xticks(positions, [f"{thresholds[i]:.3f}" for i in positions])
        ax.set(
            xlabel="Threshold relative to baseline (V)",
            ylabel="FEB index : channel",
            title=label + "Complete measurements only",
        )
        fig.colorbar(image, ax=ax, label="log10(1 + recorded hits/s)")
        fig.tight_layout()
        for ext in ("png", "pdf", "svg"):
            fig.savefig(output / f"rate_heatmap.{ext}", dpi=180)
        pdf.savefig(fig)
        plt.close(fig)

        for start in range(0, len(keys), 16):
            fig, axes = plt.subplots(4, 4, figsize=(12, 11))
            for ax, key in zip(axes.flat, keys[start : start + 16]):
                selected = sorted(
                    [r for r in rows if (int(r["feb"]), int(r["channel"])) == key],
                    key=lambda r: float(r["threshold_v"]),
                )
                good = [r for r in selected if r["status"] == "complete"]
                limited = [
                    r
                    for r in selected
                    if r["status"] in ("rate_limited", "hit_limited", "byte_limited")
                ]
                missing = [
                    r
                    for r in selected
                    if r["status"]
                    not in ("complete", "rate_limited", "hit_limited", "byte_limited")
                ]
                # NaNs break lines across unmeasured/limited points.
                ax.plot(
                    [float(r["threshold_v"]) for r in selected],
                    [
                        (
                            float(r["observed_rate_hz"])
                            if r["status"] == "complete"
                            else math.nan
                        )
                        for r in selected
                    ],
                    "o-",
                    color="#0072B2",
                    ms=2.5,
                    lw=1,
                )
                if limited:
                    ax.scatter(
                        [float(r["threshold_v"]) for r in limited],
                        [float(r["observed_rate_hz"]) for r in limited],
                        marker="^",
                        color="#D55E00",
                        s=22,
                    )
                if missing:
                    ax.scatter(
                        [float(r["threshold_v"]) for r in missing],
                        [0.03] * len(missing),
                        marker="x",
                        color="#777777",
                        s=15,
                        transform=ax.get_xaxis_transform(),
                    )
                zeros = [
                    r for r in good if int(r["hits"]) == 0 and r["zero_hit_upper_95_hz"]
                ]
                if zeros:
                    ax.vlines(
                        [float(r["threshold_v"]) for r in zeros],
                        0,
                        [float(r["zero_hit_upper_95_hz"]) for r in zeros],
                        color="#0072B2",
                        alpha=0.15,
                        lw=0.7,
                    )
                ax.set_yscale("symlog", linthresh=1)
                ax.set_ylim(bottom=0)
                path = selected[0]["physical_path"]
                ax.set_title(
                    f"FEB {key[0]} (path {path}), channel {key[1]}", fontsize=9
                )
                ax.set_xlabel("Relative threshold (V)", fontsize=8)
                ax.set_ylabel("Recorded hits/s", fontsize=8)
                ax.tick_params(labelsize=7)
                ax.grid(alpha=0.2)
            for ax in list(axes.flat)[len(keys[start : start + 16]) :]:
                ax.set_visible(False)
            fig.suptitle(
                label
                + "Per-channel response — blue complete, orange limited, bottom × unmeasured",
                fontsize=12,
            )
            fig.text(
                0.03,
                0.005,
                "Pale blue lines: Poisson 95% upper bounds for complete zero-hit samples. No automatic threshold selection.",
                fontsize=8,
            )
            fig.tight_layout(rect=(0, 0.025, 1, 0.96))
            pdf.savefig(fig)
            fig.savefig(
                output / f"channels_{start+1:03d}_{min(start+16,len(keys)):03d}.png",
                dpi=150,
            )
            plt.close(fig)
    print(f"Report: {report}")
    return report
