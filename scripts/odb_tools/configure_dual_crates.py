#!/usr/bin/env python3
"""Configure the two PIONEER SAMPIC frontend-to-crate connections in ODB."""

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from midas_client_utils import create_midas_client

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class CrateConnection:
    name: str
    frontend_index: int
    ip_address: str
    port: int
    calibration_directory: str


DEFAULT_CRATES = (
    CrateConnection(
        "N1",
        0,
        "192.168.0.13",
        27013,
        "resources/calib/Crate_PIONEER_N1_LPNHE",
    ),
    CrateConnection(
        "N2",
        1,
        "192.168.0.14",
        27014,
        "resources/calib/Crate_PIONEER_N2_LPNHE",
    ),
)


def connection_writes(crate: CrateConnection):
    root = (
        f"/Equipment/SAMPIC {crate.frontend_index:02d}/Settings/"
        "Sampic Controller"
    )
    return (
        (f"{root}/init_modes/default/ip_address", crate.ip_address),
        (f"{root}/init_modes/default/port", crate.port),
        (
            f"{root}/init_modes/default/calibration_directory",
            crate.calibration_directory,
        ),
        (
            f"{root}/apply_modes/default/calibration_directory",
            crate.calibration_directory,
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Write the settings to ODB; the default is a dry-run.",
    )
    args = parser.parse_args()

    for crate in DEFAULT_CRATES:
        calibration = REPOSITORY_ROOT / crate.calibration_directory
        if not calibration.is_dir():
            parser.error(
                f"{crate.name} calibration directory does not exist: "
                f"{calibration}"
            )

    if not args.apply:
        for crate in DEFAULT_CRATES:
            print(
                f"[DRY-RUN] frontend {crate.frontend_index:02d} -> "
                f"{crate.name} {crate.ip_address}:{crate.port}"
            )
            for path, value in connection_writes(crate):
                print(f"  {path} <- {value!r}")
        print("No ODB fields changed. Re-run with --apply to write them.")
        return 0

    try:
        client = create_midas_client("configure_sampic_crates")
    except RuntimeError as error:
        parser.error(str(error))

    try:
        for crate in DEFAULT_CRATES:
            for path, value in connection_writes(crate):
                client.odb_set(path, value, create_if_needed=False)
            print(
                f"frontend {crate.frontend_index:02d} -> {crate.name} "
                f"{crate.ip_address}:{crate.port} "
                f"({crate.calibration_directory})"
            )
    except Exception as error:  # pylint: disable=broad-except
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    finally:
        client.disconnect()

    print("Dual-crate connection settings applied. Restart both frontends.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
