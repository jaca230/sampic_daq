#!/usr/bin/env python3
"""PySequencer version of the SAMPIC pulser-period / frames-per-block scan."""
from __future__ import annotations

import json
from collections import OrderedDict
from datetime import datetime, timezone
from pathlib import Path
from typing import List, Tuple

# Absolute path so PySequencer (which runs from MIDAS userfiles) can find helpers
REPO_ROOT = Path("/home/pioneer/jcarlton/projects/midas_sampic/experiments/sampic_daq")
HELPERS_DIR = REPO_ROOT / "scripts" / "sequencer_helpers"
PARAMETER_FILE = HELPERS_DIR / "sampic_parameter_scan_params.txt"
SNAPSHOT_FILE = HELPERS_DIR / "rate_data" / "sampic_odb_samples.json"

PULSER_PERIOD_PATH = "/Equipment/SAMPIC 00/Settings/Crate/pulser_period"
FRAMES_PER_BLOCK_PATH = "/Equipment/SAMPIC 00/Settings/Crate/frames_per_block"
HELPERS_BASE = "/Sequencer/Helpers"
SUCCESS_TOKEN = "SUCCESS"
WAIT_BEFORE_SAMPLE = 3
WAIT_AFTER_STOP = 2

ODB_PATHS = [
    "/Equipment/SAMPIC 00/Settings/Crate",
    "/Equipment/SAMPIC 00/Statistics",
    "/Equipment/SAMPIC 00/Settings/Errors",
    "/Equipment/SAMPIC 00/Settings/Collector",
    "/Sequencer/Helpers",
    "/Runinfo",
]


def load_parameter_space() -> List[Tuple[int, int, int]]:
    combos: List[Tuple[int, int, int]] = []
    with PARAMETER_FILE.open() as source:
        for raw_line in source:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                raise ValueError(f"Malformed line in {PARAMETER_FILE}: '{raw_line.rstrip()}'")
            pulser_period_us, frames_per_block, duration_s = map(int, parts[:3])
            combos.append((pulser_period_us, frames_per_block, duration_s))
    if not combos:
        raise ValueError(f"No parameter combinations found in {PARAMETER_FILE}")
    return combos


def append_snapshot(snapshot: dict) -> None:
    SNAPSHOT_FILE.parent.mkdir(parents=True, exist_ok=True)
    samples = []
    if SNAPSHOT_FILE.exists():
        with SNAPSHOT_FILE.open("r", encoding="utf-8") as infile:
            try:
                samples = json.load(infile)
            except json.JSONDecodeError:
                samples = []
    samples.append(snapshot)
    with SNAPSHOT_FILE.open("w", encoding="utf-8") as outfile:
        json.dump(samples, outfile, indent=4)


def capture_snapshot(seq) -> str:
    """Pull the requested ODB paths and append them to the JSON log."""
    snapshot = OrderedDict()
    snapshot["timestamp_utc"] = datetime.now(timezone.utc).isoformat()
    for path in ODB_PATHS:
        try:
            snapshot[path] = seq.odb_get(path)
        except Exception as exc:  # pylint: disable=broad-except
            snapshot[path] = {"error": str(exc)}
    append_snapshot(snapshot)
    return SUCCESS_TOKEN


def cleanup_helper_keys(seq) -> None:
    for key in ("sequence_index", "sample_duration_seconds"):
        path = f"{HELPERS_BASE}/{key}"
        if seq.odb_exists(path):
            seq.odb_delete(path)


def define_params(seq) -> None:
    seq.register_param("start_sequence_index", "Index to start at (1-based)", 1)
    seq.register_param("end_sequence_index", "Index to stop at (<= 66)", 66)


def sequence(seq) -> None:
    combos = load_parameter_space()
    total = len(combos)

    start_index = int(seq.get_param("start_sequence_index"))
    end_index = int(seq.get_param("end_sequence_index"))

    if end_index > total or start_index < 1 or start_index > end_index:
        raise ValueError(
            f"Invalid index range start={start_index}, end={end_index}, total={total}"
        )

    iterations = end_index - start_index + 1

    try:
        seq.odb_set(f"{HELPERS_BASE}/sequence_index", start_index)
        seq.odb_set(f"{HELPERS_BASE}/sample_duration_seconds", 15)

        for offset in seq.range(iterations):
            sequence_index = start_index + offset
            pulser_period_us, frames_per_block, duration_s = combos[sequence_index - 1]

            seq.msg(
                f"Sequence index {sequence_index}/{total}: pulser={pulser_period_us}us, "
                f"frames={frames_per_block}, duration={duration_s}s"
            )

            seq.odb_set(f"{HELPERS_BASE}/sequence_index", sequence_index)
            seq.odb_set(f"{HELPERS_BASE}/sample_duration_seconds", duration_s)
            seq.odb_set(f"{HELPERS_BASE}/current_pulser_period_us", pulser_period_us)
            seq.odb_set(f"{HELPERS_BASE}/current_frames_per_block", frames_per_block)

            seq.odb_set(PULSER_PERIOD_PATH, pulser_period_us)
            seq.odb_set(FRAMES_PER_BLOCK_PATH, frames_per_block)

            seq.start_run()
            seq.wait_seconds(WAIT_BEFORE_SAMPLE)
            seq.wait_seconds(duration_s)

            result = capture_snapshot(seq)
            if result != SUCCESS_TOKEN:
                raise RuntimeError("Snapshot capture failed")

            seq.stop_run()
            seq.wait_seconds(WAIT_AFTER_STOP)

    finally:
        cleanup_helper_keys(seq)


def at_exit(seq) -> None:
    cleanup_helper_keys(seq)
