#!/usr/bin/env python3
"""Plot throughput for the collection, cuts, and finalization pipeline."""
import argparse
import json
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.input.read_text())
    runs = document.get("runs", document)
    rows = []
    for name, value in runs.items():
        for run in value if isinstance(value, list) else [value]:
            if not isinstance(run, dict): continue
            for stage, totals in run.get("stages", {}).items():
                rows.append((name, stage, totals.get("service_hits_per_second", 0)))
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(10, max(3, len(rows)*0.3)))
    ax.barh([f"{name}: {stage} ({i})" for i, (name,stage,_) in enumerate(rows)],
            [rate for _,_,rate in rows])
    ax.set_xlabel("Stage service capacity (hits/s)")
    ax.set_title("SAMPIC collection → cuts → finalization")
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output)
    plt.close(fig)
if __name__ == "__main__": main()
