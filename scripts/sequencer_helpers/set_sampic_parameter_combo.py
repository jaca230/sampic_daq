#!/usr/bin/env python3
"""Set SAMPIC pulser/frames settings based on the current sequencer index."""
import os
import sys
from pathlib import Path
from typing import List, Tuple


def _ensure_midas_client_importable() -> None:
    midas_sys = os.environ.get("MIDASSYS")
    if not midas_sys:
        return
    candidate = Path(midas_sys) / "python"
    if candidate.is_dir():
        sys.path.insert(0, str(candidate))


_ensure_midas_client_importable()

try:
    import midas.client  # type: ignore
except ModuleNotFoundError as exc:  # pragma: no cover - runtime guard
    print(f"Error: unable to import midas.client ({exc})", flush=True)
    sys.exit(1)

PARAMETER_FILE = Path(__file__).resolve().parent / "sampic_parameter_scan_params.txt"
PULSER_PERIOD_PATH = "/Equipment/SAMPIC 00/Settings/Crate/pulser_period"
FRAMES_PER_BLOCK_PATH = "/Equipment/SAMPIC 00/Settings/Crate/frames_per_block"
HELPERS_BASE = "/Sequencer/Helpers"


def load_parameter_space() -> List[Tuple[int, int, int]]:
    """Return list of (pulser_period_us, frames_per_block, duration_s)."""
    combos: List[Tuple[int, int, int]] = []
    with PARAMETER_FILE.open() as source:
        for raw_line in source:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                raise ValueError(f"Malformed line in {PARAMETER_FILE}: '{raw_line.rstrip()}'")
            pulser_period_us, frames_per_block, duration_s = parts[:3]
            combos.append((int(pulser_period_us), int(frames_per_block), int(duration_s)))
    if not combos:
        raise ValueError(f"No parameter combinations found in {PARAMETER_FILE}")
    return combos


def set_parameters_from_index() -> None:
    combos = load_parameter_space()
    client = midas.client.MidasClient("sampic_parameter_setter")

    try:
        sequence_index = client.odb_get(f"{HELPERS_BASE}/sequence_index")
        if sequence_index is None:
            raise RuntimeError("/Sequencer/Helpers/sequence_index is not set")

        zero_index = int(sequence_index) - 1  # MIDAS stores indices as 1-based
        if zero_index < 0 or zero_index >= len(combos):
            raise IndexError(
                f"sequence_index {sequence_index} (zero-based {zero_index}) is outside the parameter space"
            )

        pulser_period_us, frames_per_block, duration_s = combos[zero_index]

        client.odb_set(PULSER_PERIOD_PATH, pulser_period_us)
        client.odb_set(FRAMES_PER_BLOCK_PATH, frames_per_block)

        # Record helper information for bookkeeping and downstream scripts.
        client.odb_set(f"{HELPERS_BASE}/current_pulser_period_us", pulser_period_us)
        client.odb_set(f"{HELPERS_BASE}/current_frames_per_block", frames_per_block)
        client.odb_set(f"{HELPERS_BASE}/sample_duration_seconds", duration_s)

        print(
            "SUCCESS: Set pulser_period_us=", pulser_period_us,
            "frames_per_block=", frames_per_block,
            "duration_s=", duration_s,
            "sequence_index=", sequence_index,
        )
    except Exception as exc:  # pylint: disable=broad-except
        print(f"Error: {exc}")
        raise
    finally:
        client.disconnect()


if __name__ == "__main__":
    set_parameters_from_index()
