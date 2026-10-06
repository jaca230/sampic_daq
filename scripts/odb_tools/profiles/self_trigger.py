"""Plain per-channel self-trigger hardware profile."""

from argparse import ArgumentParser, Namespace
from dataclasses import dataclass
from typing import Sequence

from profiles.profile_definition import (
    OdbProfile,
    OdbWrite,
    correction_writes,
    settings_root,
)


BOARD_COUNT = 4
CHANNELS_PER_BOARD = 64
CHANNELS_PER_SAMPIC = 16


@dataclass(frozen=True, order=True)
class ChannelAddress:
    """One channel expressed using FEB-wide channel numbering."""

    feb: int
    channel: int

    @property
    def sampic(self) -> int:
        return self.channel // CHANNELS_PER_SAMPIC

    @property
    def local_channel(self) -> int:
        return self.channel % CHANNELS_PER_SAMPIC


def parse_channels(raw: str) -> list[ChannelAddress]:
    """Parse ``all`` or a comma-separated FEB:CHANNEL selection."""
    if raw.strip().lower() in {"all", "*"}:
        return [
            ChannelAddress(feb, channel)
            for feb in range(BOARD_COUNT)
            for channel in range(CHANNELS_PER_BOARD)
        ]

    result = set()
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        parts = token.split(":")
        if len(parts) != 2:
            raise ValueError(f"channel '{token}' must use FEB:CHANNEL")
        try:
            address = ChannelAddress(int(parts[0]), int(parts[1]))
        except ValueError as error:
            raise ValueError(
                f"channel '{token}' must contain integers"
            ) from error
        if address.feb < 0 or address.feb >= BOARD_COUNT:
            raise ValueError(f"FEB in '{token}' must be in [0, 3]")
        if address.channel < 0 or address.channel >= CHANNELS_PER_BOARD:
            raise ValueError(f"channel in '{token}' must be in [0, 63]")
        result.add(address)
    if not result:
        raise ValueError("at least one channel is required")
    return sorted(result)


class SelfTriggerProfile(OdbProfile):
    """Select only named channels and remove external-gate trigger state."""

    name = "self_trigger"
    description = (
        "Configure a clean per-channel self-trigger setup and disable all "
        "unselected channels."
    )

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument(
            "--channels",
            required=True,
            metavar="FEB:CHANNEL[,...]",
            help=(
                "Enabled channels using FEB-wide numbering, for example "
                "0:3,0:14,3:48, or 'all'. All unselected channels are "
                "disabled."
            ),
        )
        parser.add_argument("--threshold-volts", type=float, default=0.15)
        parser.add_argument(
            "--trigger-edge",
            type=int,
            choices=(0, 1),
            default=0,
            help="Self-trigger edge: rising=0 (default), falling=1.",
        )
        parser.add_argument(
            "--negative-pulse",
            action="store_true",
            help="Use negative-pulse mode instead of positive-pulse mode.",
        )

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if not 0.0 <= arguments.threshold_volts <= 1.0:
            raise ValueError("threshold must be in [0, 1] V")
        selected = set(parse_channels(arguments.channels))
        root = settings_root(arguments.frontend_index)

        writes = correction_writes(root) + [
            OdbWrite(
                f"{root}/Crate/external_trigger_type",
                0,
                "Restore the software/default external-trigger source.",
            ),
            OdbWrite(
                f"{root}/Crate/external_trigger_counter/enabled",
                False,
                "Disable external-trigger counting.",
            ),
            OdbWrite(
                f"{root}/Crate/external_trigger_counter/detect_trigger_id",
                False,
                "Disable external trigger-ID decoding.",
            ),
            OdbWrite(
                f"{root}/Crate/level2_trigger_build",
                False,
                "Disable FEB Level-2 external-gated event construction.",
            ),
            OdbWrite(
                f"{root}/Crate/level3_trigger_build",
                False,
                "Disable crate Level-3 trigger construction.",
            ),
        ]

        for feb in range(BOARD_COUNT):
            feb_root = f"{root}/Crate/front_end_boards/feb{feb}"
            writes.extend(
                [
                    OdbWrite(
                        f"{feb_root}/global_trigger_option",
                        0,
                        "Use the normal per-channel trigger path.",
                    ),
                    OdbWrite(
                        f"{feb_root}/level2_coincidence_ext_gate",
                        False,
                        "Remove the FEB external-gate coincidence requirement.",
                    ),
                    OdbWrite(
                        f"{feb_root}/level2_trigger_logic/apply",
                        False,
                        "Do not apply Level-2 coincidence logic.",
                    ),
                ]
            )

            for sampic in range(CHANNELS_PER_BOARD // CHANNELS_PER_SAMPIC):
                chip_root = f"{feb_root}/sampics/sampic{sampic}"
                writes.extend(
                    [
                        OdbWrite(
                            f"{chip_root}/trigger_option",
                            0,
                            "Restore the normal SAMPIC trigger option.",
                        ),
                        OdbWrite(
                            f"{chip_root}/enable_trigger/use_external",
                            False,
                            "Disable chip-level external trigger gating.",
                        ),
                        OdbWrite(
                            f"{chip_root}/enable_trigger/open_gate_on_external",
                            False,
                            "Do not open the channel gate from an external input.",
                        ),
                    ]
                )

                for local_channel in range(CHANNELS_PER_SAMPIC):
                    address = ChannelAddress(
                        feb,
                        sampic * CHANNELS_PER_SAMPIC + local_channel,
                    )
                    enabled = address in selected
                    channel_root = (
                        f"{chip_root}/channels/channel{local_channel}"
                    )
                    writes.extend(
                        [
                            OdbWrite(
                                f"{channel_root}/enabled",
                                enabled,
                                "Enable only explicitly selected channels.",
                            ),
                            OdbWrite(
                                f"{channel_root}/enable_for_central_trigger",
                                enabled,
                                "Match trigger participation to channel enablement.",
                            ),
                            OdbWrite(
                                f"{channel_root}/trigger_mode",
                                0 if enabled else 3,
                                "Use self-trigger mode only on selected channels.",
                            ),
                            OdbWrite(
                                f"{channel_root}/trigger_edge",
                                arguments.trigger_edge,
                                "Set the self-trigger edge.",
                            ),
                            OdbWrite(
                                f"{channel_root}/pulse_mode",
                                not arguments.negative_pulse,
                                "Set positive- or negative-pulse discrimination.",
                            ),
                            OdbWrite(
                                f"{channel_root}/internal_threshold",
                                arguments.threshold_volts,
                                "Set a deterministic threshold on every channel.",
                            ),
                        ]
                    )

        return writes


PROFILE = SelfTriggerProfile()
