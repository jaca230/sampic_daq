"""L2 external-gate ODB profile."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import (
    OdbProfile,
    OdbWrite,
    parse_index_selection,
    settings_root,
)


class L2ExternalTriggerProfile(OdbProfile):
    """Self-trigger channels while requiring the FEB L2 external gate."""

    name = "l2_external_trigger"
    description = (
        "Self-trigger SAMPIC channels with FEB L2 external gating and "
        "external-trigger event building."
    )

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("--boards", default="0")
        parser.add_argument("--chips", default="all")
        parser.add_argument("--channels", default="all")
        parser.add_argument(
            "--ext-trigger-type",
            type=int,
            default=4,
            help="ExternalTriggerType_t value (EXT_SIG=4).",
        )
        parser.add_argument(
            "--signal-level",
            type=int,
            default=0,
            help="SignalLevel_t value (TTL_SIG=0).",
        )
        parser.add_argument(
            "--trigger-edge",
            type=int,
            default=0,
            help="EdgeType_t value (RISING_EDGE=0).",
        )
        parser.add_argument("--primitive-gate-length", type=int, default=10)
        parser.add_argument("--latency-gate-length", type=int, default=3)
        parser.add_argument("--level2-ext-gate", type=int, default=5)
        parser.add_argument("--hit-time-offset-ns", type=float, default=-470.0)
        parser.add_argument("--pre-window-ns", type=float, default=20.0)
        parser.add_argument("--post-window-ns", type=float, default=20.0)

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        boards = parse_index_selection(arguments.boards, 4, "board")
        chips = parse_index_selection(arguments.chips, 4, "chip")
        channels = parse_index_selection(
            arguments.channels, 16, "channel"
        )
        root = settings_root(arguments.frontend_index)

        writes = [
            OdbWrite(
                f"{root}/Crate/external_trigger_type",
                arguments.ext_trigger_type,
                "Use the configured external-trigger input type.",
            ),
            OdbWrite(
                f"{root}/Crate/signal_level",
                arguments.signal_level,
                "Use the configured external-trigger electrical level.",
            ),
            OdbWrite(
                f"{root}/Crate/trigger_edge",
                arguments.trigger_edge,
                "Trigger on the configured external-signal edge.",
            ),
            OdbWrite(
                f"{root}/Crate/primitives_gate_length",
                arguments.primitive_gate_length,
                "Set the central-trigger primitive coincidence gate.",
            ),
            OdbWrite(
                f"{root}/Crate/latency_gate_length",
                arguments.latency_gate_length,
                "Set the central-trigger latency gate.",
            ),
            OdbWrite(
                f"{root}/Crate/level2_trigger_build",
                True,
                "Enable FEB level-2 trigger construction.",
            ),
            OdbWrite(
                f"{root}/Crate/external_trigger_counter/enabled",
                True,
                "Enable the external-trigger counter.",
            ),
            OdbWrite(
                f"{root}/Crate/external_trigger_counter/detect_trigger_id",
                True,
                "Record external trigger identifiers.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/mode",
                "external_trigger",
                "Build frontend events around external-trigger timestamps.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_trigger/hit_time_offset_ns",
                arguments.hit_time_offset_ns,
                "Align hit timestamps with the external trigger.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_trigger/pre_window_ns",
                arguments.pre_window_ns,
                "Accept hits this far before the aligned trigger.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_trigger/post_window_ns",
                arguments.post_window_ns,
                "Accept hits this far after the aligned trigger.",
            ),
        ]

        for board in boards:
            board_root = (
                f"{root}/Crate/front_end_boards/feb{board}"
            )
            writes.extend(
                [
                    OdbWrite(
                        f"{board_root}/global_trigger_option",
                        0,
                        "Use the FEB channel-trigger path.",
                    ),
                    OdbWrite(
                        f"{board_root}/level2_coincidence_ext_gate",
                        True,
                        "Require the external gate in FEB L2 coincidence.",
                    ),
                    OdbWrite(
                        f"{board_root}/level2_ext_trig_gate",
                        arguments.level2_ext_gate,
                        "Set the FEB L2 external-gate width.",
                    ),
                    OdbWrite(
                        f"{board_root}/level2_trigger_logic/apply",
                        True,
                        "Apply the configured FEB L2 trigger logic.",
                    ),
                ]
            )

            for chip in chips:
                chip_root = (
                    f"{board_root}/sampics/sampic{chip}"
                )
                writes.append(
                    OdbWrite(
                        f"{chip_root}/trigger_option",
                        1,
                        "Enable the SAMPIC channel-trigger option.",
                    )
                )

                for channel in channels:
                    channel_root = (
                        f"{chip_root}/channels/channel{channel}"
                    )
                    writes.extend(
                        [
                            OdbWrite(
                                f"{channel_root}/trigger_mode",
                                0,
                                "Use self-trigger mode for this channel.",
                            ),
                            OdbWrite(
                                f"{channel_root}/"
                                "enable_for_central_trigger",
                                True,
                                "Include this channel in central triggering.",
                            ),
                        ]
                    )

        return writes


PROFILE = L2ExternalTriggerProfile()
