#!/usr/bin/env python3
"""Save the complete SAMPIC frontend ODB subtree to an ignored local artifact."""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from midas_client_utils import create_midas_client


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-index", type=int, default=0)
    parser.add_argument("--output-dir", default=".artifacts/odb_snapshots")
    args = parser.parse_args()
    try:
        client = create_midas_client("snapshot_sampic_odb")
    except RuntimeError as error:
        parser.error(str(error))
    path = f"/Equipment/SAMPIC {args.frontend_index:02d}"
    try:
        snapshot = client.odb_get(path)
    finally:
        client.disconnect()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = output_dir / f"sampic_{args.frontend_index:02d}_{stamp}.json"
    output.write_text(json.dumps(snapshot, indent=2, sort_keys=True, default=str) + "\n")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
