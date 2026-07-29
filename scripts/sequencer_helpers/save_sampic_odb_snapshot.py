#!/usr/bin/env python3
"""Read relevant SAMPIC ODB nodes and append them to rate_data/sampic_odb_samples.json."""
import json
import os
import sys
from collections import OrderedDict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def _ensure_midas_client_importable() -> None:
    """Guarantee MIDASSYS/python is on sys.path before importing midas."""
    midas_sys = os.environ.get("MIDASSYS")
    if not midas_sys:
        return
    candidate = Path(midas_sys) / "python"
    if candidate.is_dir():
        sys.path.insert(0, str(candidate))


_ensure_midas_client_importable()

try:
    import midas.client  # type: ignore
except ModuleNotFoundError as exc:  # pragma: no cover - handled at runtime
    print(f"ERROR: unable to import midas.client ({exc})", flush=True)
    sys.exit(1)

SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_FILE = SCRIPT_DIR / "rate_data" / "sampic_odb_samples.json"

ODB_PATHS = [
    "/Equipment/SAMPIC 00/Statistics",
    "/Equipment/SAMPIC 00/Settings/Sampic Controller",
    "/Equipment/SAMPIC 00/Settings/Sampic Event Collector",
    "/Sequencer/Helpers",
    "/Runinfo",
]

CRATE_FIELDS = {
    "pulser_period": "/Equipment/SAMPIC 00/Settings/Crate/pulser_period",
    "frames_per_block": "/Equipment/SAMPIC 00/Settings/Crate/frames_per_block",
}


def convert_ordered_dict(obj: Any) -> Any:
    """Recursively convert OrderedDicts to plain dicts to make JSON serialization deterministic."""
    if isinstance(obj, OrderedDict):
        return {key: convert_ordered_dict(value) for key, value in obj.items()}
    if isinstance(obj, list):
        return [convert_ordered_dict(item) for item in obj]
    return obj


def append_sample(data: dict) -> None:
    """Append the sample to OUTPUT_FILE, preserving existing entries if present."""
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    samples = []
    if OUTPUT_FILE.exists():
        with OUTPUT_FILE.open("r", encoding="utf-8") as infile:
            try:
                samples = json.load(infile)
            except json.JSONDecodeError:
                # Fall back to a fresh list if the file is corrupted.
                samples = []
    samples.append(data)
    with OUTPUT_FILE.open("w", encoding="utf-8") as outfile:
        json.dump(samples, outfile, indent=4)


def capture_odb_snapshot() -> None:
    """Fetch all ODB_PATHS, store results, and append a snapshot sample."""
    client = midas.client.MidasClient("sampic_odb_snapshot")
    snapshot = OrderedDict()
    snapshot["timestamp_utc"] = datetime.now(timezone.utc).isoformat()

    try:
        for path in ODB_PATHS:
            try:
                value = client.odb_get(path)
                snapshot[path] = convert_ordered_dict(value)
            except Exception as exc:  # pylint: disable=broad-except
                snapshot[path] = {"error": str(exc)}

        crate_snapshot = {}
        for label, key in CRATE_FIELDS.items():
            try:
                crate_snapshot[label] = client.odb_get(key)
            except Exception as exc:  # pylint: disable=broad-except
                crate_snapshot[label] = {"error": str(exc)}
        snapshot["/Equipment/SAMPIC 00/Settings/Crate"] = crate_snapshot

        append_sample(snapshot)
    finally:
        client.disconnect()


def main() -> None:
    try:
        capture_odb_snapshot()
    except Exception as exc:  # pylint: disable=broad-except
        print(f"ERROR: {exc}", flush=True)
        sys.exit(1)
    else:
        # Sequencer checks explicitly for this exact string.
        print("SUCCESS", end="", flush=True)


if __name__ == "__main__":
    main()
