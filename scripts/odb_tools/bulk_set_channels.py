#!/usr/bin/env python3

"""
Bulk update channel-level parameters in the SAMPIC frontend ODB tree.

Example usage:

    ./bulk_set_channels.py enabled true
    ./bulk_set_channels.py internal_threshold 0.12 --chips 0,1 --boards all
"""

import argparse
import sys
from typing import Iterable, List, Sequence

import midas.client

FRONTEND_NAME_TEMPLATE = "SAMPIC {index:02d}"
SETTINGS_ROOT = "/Equipment/{frontend}/Settings/Crate/front_end_boards"
BOARD_COUNT = 4
CHIP_COUNT = 4
CHANNEL_COUNT = 16


def parse_index_selection(raw: str, upper_bound: int, label: str) -> Sequence[int]:
    lowered = raw.lower()
    if lowered in {"all", "*"}:
        return list(range(upper_bound))

    indices: List[int] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        try:
            value = int(token, 10)
        except ValueError as exc:
            raise ValueError(f"Invalid {label} index '{token}'") from exc
        if value < 0 or value >= upper_bound:
            raise ValueError(f"{label} index {value} out of range [0, {upper_bound - 1}]")
        indices.append(value)
    if not indices:
        raise ValueError(f"No {label} indices provided in '{raw}'")
    return sorted(set(indices))


def parse_value(text: str):
    lowered = text.lower()
    if lowered in {"true", "false"}:
        return lowered == "true"
    try:
        return int(text, 10)
    except ValueError:
        try:
            return float(text)
        except ValueError:
            return text


def build_paths(frontend: str,
                boards: Iterable[int],
                chips: Iterable[int],
                channels: Iterable[int],
                field: str) -> Iterable[str]:
    base = SETTINGS_ROOT.format(frontend=frontend)
    for board in boards:
        for chip in chips:
            for channel in channels:
                yield (
                    f"{base}/feb{board}/sampics/sampic{chip}/channels/"
                    f"channel{channel}/{field}"
                )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bulk update SAMPIC channel-level ODB parameters."
    )
    parser.add_argument(
        "field",
        help="Channel field name to update (e.g. enabled, internal_threshold).",
    )
    parser.add_argument(
        "value",
        help="Value to write. Booleans use true/false, numbers are parsed automatically.",
    )
    parser.add_argument(
        "--frontend-index",
        type=int,
        default=0,
        help="Frontend index used in /Equipment/SAMPIC XX (default: 0).",
    )
    parser.add_argument(
        "--boards",
        default="all",
        help="Comma-separated list of board indices (0-3) or 'all'.",
    )
    parser.add_argument(
        "--chips",
        default="all",
        help="Comma-separated list of chip indices (0-3) or 'all'.",
    )
    parser.add_argument(
        "--channels",
        default="all",
        help="Comma-separated list of channel indices (0-15) or 'all'.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the operations without modifying the ODB.",
    )

    args = parser.parse_args()

    try:
        boards = parse_index_selection(args.boards, BOARD_COUNT, "board")
        chips = parse_index_selection(args.chips, CHIP_COUNT, "chip")
        channels = parse_index_selection(args.channels, CHANNEL_COUNT, "channel")
    except ValueError as exc:
        parser.error(str(exc))

    value = parse_value(args.value)
    frontend = FRONTEND_NAME_TEMPLATE.format(index=args.frontend_index)
    paths = list(build_paths(frontend, boards, chips, channels, args.field))

    if not paths:
        print("Nothing to update.")
        return 0

    client = midas.client.MidasClient("sampic_channel_bulk_update")

    try:
        for path in paths:
            if args.dry_run:
                print(f"[DRY-RUN] {path} <- {value!r}")
            else:
                client.odb_set(path, value)
                print(f"{path} <- {value!r}")
    except Exception as exc:  # pylint: disable=broad-except
        print(f"ERROR: {exc}")
        return 1
    finally:
        client.disconnect()

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

