#!/usr/bin/env python3
"""List or apply a named SAMPIC ODB configuration profile."""

import argparse
import sys
from pathlib import Path
from typing import Iterable

ODB_TOOLS_DIRECTORY = Path(__file__).resolve().parent.parent
if str(ODB_TOOLS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(ODB_TOOLS_DIRECTORY))

from midas_client_utils import create_midas_client  # noqa: E402
from profiles.profile_definition import OdbWrite  # noqa: E402
from profiles.profile_registry import discover_profiles  # noqa: E402


def validated_writes(writes: Iterable[OdbWrite]) -> list[OdbWrite]:
    """Reject empty paths and conflicting duplicate writes."""
    result: list[OdbWrite] = []
    values_by_path = {}

    for write in writes:
        if not write.path.startswith("/"):
            raise ValueError(
                f"profile produced a non-absolute ODB path: {write.path}"
            )
        if write.path in values_by_path:
            if values_by_path[write.path] != write.value:
                raise ValueError(
                    f"profile assigns conflicting values to {write.path}"
                )
            continue
        values_by_path[write.path] = write.value
        result.append(write)

    if not result:
        raise ValueError("profile produced no ODB writes")
    return result


def build_parser():
    """Build subcommands from all automatically discovered profiles."""
    profiles = discover_profiles()
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list", help="List available profiles.")

    for name, profile in profiles.items():
        profile_parser = subparsers.add_parser(
            name,
            help=profile.description,
            description=profile.description,
        )
        profile_parser.add_argument(
            "--frontend-index",
            type=int,
            default=0,
            help="Frontend index used in /Equipment/SAMPIC XX (default: 0).",
        )
        profile_parser.add_argument(
            "--apply",
            action="store_true",
            help="Write the profile to ODB; default is a dry-run.",
        )
        profile.configure_parser(profile_parser)

    return parser, profiles


def main() -> int:
    parser, profiles = build_parser()
    arguments = parser.parse_args()

    if arguments.command == "list":
        for name, profile in profiles.items():
            print(f"{name}: {profile.description}")
        return 0

    profile = profiles[arguments.command]
    try:
        writes = validated_writes(profile.build_writes(arguments))
    except ValueError as error:
        parser.error(str(error))

    if not arguments.apply:
        print(f"Profile: {profile.name}")
        for write in writes:
            print(
                f"[DRY-RUN] {write.path} <- {write.value!r}"
                f"  # {write.description}"
            )
        print(
            f"No ODB fields changed. Re-run with --apply to perform "
            f"these {len(writes)} writes."
        )
        return 0

    try:
        client = create_midas_client(
            f"sampic_profile_{profile.name}"[:31]
        )
    except RuntimeError as error:
        parser.error(str(error))

    try:
        for write in writes:
            client.odb_set(write.path, write.value)
            print(
                f"{write.path} <- {write.value!r}"
                f"  # {write.description}"
            )
    except Exception as error:  # pylint: disable=broad-except
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    finally:
        client.disconnect()

    print(f"Applied profile '{profile.name}' ({len(writes)} writes).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
