#!/usr/bin/env python3

"""
Bulk update board-level parameters in the SAMPIC frontend ODB tree.

Example usage:

    ./bulk_set_boards.py global_trigger_option 1
    ./bulk_set_boards.py level2_trigger_build true --boards 0,2,3
"""

import argparse
import sys
from typing import Iterable, List, Sequence

from midas_client_utils import create_midas_client

FRONTEND_NAME_TEMPLATE = "SAMPIC {index:02d}"
SETTINGS_ROOT = "/Equipment/{frontend}/Settings/Crate/front_end_boards"
BOARD_COUNT = 4


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
                field: str) -> Iterable[str]:
    base = SETTINGS_ROOT.format(frontend=frontend)
    for board in boards:
        yield f"{base}/feb{board}/{field}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bulk update SAMPIC board-level ODB parameters."
    )
    parser.add_argument(
        "field",
        help="Board field name to update (e.g. global_trigger_option).",
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
        "--dry-run",
        action="store_true",
        help="Print the operations without modifying the ODB.",
    )

    args = parser.parse_args()

    try:
        boards = parse_index_selection(args.boards, BOARD_COUNT, "board")
    except ValueError as exc:
        parser.error(str(exc))

    value = parse_value(args.value)
    frontend = FRONTEND_NAME_TEMPLATE.format(index=args.frontend_index)
    paths = list(build_paths(frontend, boards, args.field))

    if not paths:
        print("Nothing to update.")
        return 0

    if args.dry_run:
        for path in paths:
            print(f"[DRY-RUN] {path} <- {value!r}")
        print("No ODB fields changed.")
        return 0

    try:
        client = create_midas_client("sampic_board_bulk_update")
    except RuntimeError as exc:
        print(f"ERROR: {exc}")
        return 1

    try:
        for path in paths:
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
