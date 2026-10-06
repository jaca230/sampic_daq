#!/usr/bin/env python3
"""Enable a SAMPIC channel set and optionally assign its threshold in ODB.

Channels use FEB:CHANNEL notation, where CHANNEL is the FEB-wide hardware
channel number (0..63), matching the channel occupancy tool's output.
"""

import argparse
import sys
from dataclasses import dataclass
from typing import Iterable

from midas_client_utils import create_midas_client

BOARD_COUNT = 4
CHANNELS_PER_BOARD = 64
CHANNELS_PER_SAMPIC = 16


@dataclass(frozen=True, order=True)
class ChannelAddress:
    feb: int
    channel: int

    @property
    def sampic(self) -> int:
        return self.channel // CHANNELS_PER_SAMPIC

    @property
    def local_channel(self) -> int:
        return self.channel % CHANNELS_PER_SAMPIC


def parse_channels(values: Iterable[str]) -> list[ChannelAddress]:
    result = set()
    for value in values:
        for token in value.split(","):
            token = token.strip()
            if not token:
                continue
            parts = token.split(":")
            if len(parts) != 2:
                raise ValueError(f"channel '{token}' must use FEB:CHANNEL")
            try:
                address = ChannelAddress(int(parts[0]), int(parts[1]))
            except ValueError as error:
                raise ValueError(f"channel '{token}' must contain integers") from error
            if address.feb < 0 or address.feb >= BOARD_COUNT:
                raise ValueError(f"FEB in '{token}' must be in [0, 3]")
            if address.channel < 0 or address.channel >= CHANNELS_PER_BOARD:
                raise ValueError(f"channel in '{token}' must be in [0, 63]")
            result.add(address)
    if not result:
        raise ValueError("at least one channel is required")
    return sorted(result)


def all_channels() -> Iterable[ChannelAddress]:
    for feb in range(BOARD_COUNT):
        for channel in range(CHANNELS_PER_BOARD):
            yield ChannelAddress(feb, channel)


def channel_root(frontend_index: int, address: ChannelAddress) -> str:
    return (
        f"/Equipment/SAMPIC {frontend_index:02d}/Settings/Crate/"
        f"front_end_boards/feb{address.feb}/sampics/sampic{address.sampic}/"
        f"channels/channel{address.local_channel}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--frontend-index", type=int)
    target.add_argument(
        "--crate",
        choices=("n1", "n2"),
        help="Convenience alias: n1 selects frontend 00; n2 selects 01.",
    )
    parser.add_argument(
        "--channels",
        action="append",
        required=True,
        metavar="FEB:CHANNEL[,...]",
        help="FEB-wide channel identities; repeat or use a comma-separated list.",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        help="Set internal_threshold for every selected channel.",
    )
    parser.add_argument(
        "--disable-others",
        action="store_true",
        help="Disable every channel not listed before enabling the selection.",
    )
    parser.add_argument(
        "--leave-central-trigger",
        action="store_true",
        help="Do not mirror enabled state to enable_for_central_trigger.",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Write to ODB; the default is a dry-run.",
    )
    args = parser.parse_args()

    frontend_index = (
        {"n1": 0, "n2": 1}[args.crate]
        if args.crate is not None
        else args.frontend_index
    )
    if frontend_index < 0 or frontend_index > 99:
        parser.error("frontend index must be in [0, 99]")
    if args.threshold is not None and not 0.0 <= args.threshold <= 1.0:
        parser.error("threshold must be in [0, 1] V")
    try:
        selected = parse_channels(args.channels)
    except ValueError as error:
        parser.error(str(error))

    selected_set = set(selected)
    writes = []
    if args.disable_others:
        for address in all_channels():
            if address in selected_set:
                continue
            root = channel_root(frontend_index, address)
            writes.append((f"{root}/enabled", False))
            if not args.leave_central_trigger:
                writes.append((f"{root}/enable_for_central_trigger", False))

    for address in selected:
        root = channel_root(frontend_index, address)
        writes.append((f"{root}/enabled", True))
        if not args.leave_central_trigger:
            writes.append((f"{root}/enable_for_central_trigger", True))
        if args.threshold is not None:
            writes.append((f"{root}/internal_threshold", args.threshold))

    action = "APPLY" if args.apply else "DRY-RUN"
    print(
        f"[{action}] frontend {frontend_index:02d}: "
        f"{len(selected)} selected channel(s)"
    )
    for address in selected:
        detail = (
            f"FEB {address.feb}, channel {address.channel} "
            f"(SAMPIC {address.sampic}, local channel {address.local_channel})"
        )
        if args.threshold is not None:
            detail += f", threshold {args.threshold:g} V"
        print(f"  {detail}")
    if args.disable_others:
        print(f"  disabling {BOARD_COUNT * CHANNELS_PER_BOARD - len(selected)} other channels")

    if not args.apply:
        print(f"No ODB fields changed ({len(writes)} pending writes).")
        return 0

    try:
        client = create_midas_client(
            f"sampic_channels_{frontend_index:02d}"
        )
    except RuntimeError as error:
        parser.error(str(error))

    try:
        for path, value in writes:
            client.odb_set(path, value, create_if_needed=False)
    except Exception as error:  # pylint: disable=broad-except
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    finally:
        client.disconnect()

    print(f"Applied {len(writes)} ODB writes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
