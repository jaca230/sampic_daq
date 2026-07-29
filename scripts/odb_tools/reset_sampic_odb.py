#!/usr/bin/env python3
"""Snapshot and delete one SAMPIC equipment subtree so the frontend can rebuild it."""

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from midas_client_utils import create_midas_client


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-index", type=int, default=0)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Actually snapshot and delete; default is a dry-run.",
    )
    args = parser.parse_args()
    if args.frontend_index < 0 or args.frontend_index > 99:
        parser.error("frontend index must be in [0, 99]")

    root = f"/Equipment/SAMPIC {args.frontend_index:02d}"
    if not args.apply:
        print(f"[DRY-RUN] snapshot {root}")
        print(f"[DRY-RUN] delete {root}")
        print("No ODB fields changed. Re-run with --apply after stopping the frontend.")
        return 0

    try:
        client = create_midas_client("reset_sampic_odb")
    except RuntimeError as error:
        parser.error(str(error))
    try:
        contents = client.odb_get(root)
        output_directory = (
            Path(__file__).resolve().parents[2]
            / ".artifacts"
            / "odb_snapshots"
        )
        output_directory.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output_path = output_directory / (
            f"sampic_{args.frontend_index:02d}_before_reset_{timestamp}.json"
        )
        with output_path.open("w", encoding="utf-8") as output:
            json.dump(contents, output, indent=2, default=str)
            output.write("\n")
        if output_path.stat().st_size == 0:
            raise RuntimeError("refusing to delete after writing an empty snapshot")

        client.odb_delete(root)
        print(f"Snapshot: {output_path}")
        print(f"Deleted: {root}")
        print("Restart the frontend to regenerate the equipment and Settings trees.")
        return 0
    finally:
        client.disconnect()


if __name__ == "__main__":
    sys.exit(main())
