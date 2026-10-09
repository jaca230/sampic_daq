"""L2 external-gate ODB profile."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import (
    OdbProfile,
    OdbWrite,
    correction_writes,
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
        parser.add_argument("--boards", default="all")
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
        parser.add_argument("--sampling-frequency-mhz", type=int, default=6400)
        parser.add_argument(
            "--external-clock",
            action="store_true",
            help="Use the externally supplied sampling clock.",
        )
        parser.add_argument("--frames-per-block", type=int, default=31)
        parser.add_argument("--triggers-per-event", type=int, default=127)
        parser.add_argument("--threshold-volts", type=float, default=0.1)
        parser.add_argument("--sampic-buffer-size", type=int, default=1024)
        parser.add_argument("--frontend-buffer-size", type=int, default=4096)
        parser.add_argument("--hit-time-offset-ns", type=float, default=-470.0)
        parser.add_argument("--pre-window-ns", type=float, default=500.0)
        parser.add_argument("--post-window-ns", type=float, default=500.0)
        parser.add_argument("--omit-data-bank", action="store_true")
        parser.add_argument(
            "--omit-advanced-data",
            action="store_true",
            help="Do not write the compact advanced-hit bank.",
        )
        parser.add_argument("--advanced-bank-prefix", default="SH")
        parser.add_argument("--omit-event-timing-bank", action="store_true")
        parser.add_argument("--omit-trigger-metadata-bank", action="store_true")

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if arguments.frames_per_block <= 0:
            raise ValueError("frames per block must be positive")
        if arguments.triggers_per_event < 1 or arguments.triggers_per_event > 127:
            raise ValueError("triggers per event must be in [1, 127]")
        if arguments.sampling_frequency_mhz <= 0:
            raise ValueError("sampling frequency must be positive")
        if len(arguments.advanced_bank_prefix) != 2:
            raise ValueError("advanced bank prefix must contain exactly 2 characters")
        if not arguments.omit_advanced_data and arguments.advanced_bank_prefix == "SD":
            raise ValueError("advanced and data bank prefixes must differ")
        if (
            arguments.sampic_buffer_size <= 0
            or arguments.frontend_buffer_size <= 0
        ):
            raise ValueError("buffer sizes must be positive")
        boards = parse_index_selection(arguments.boards, 4, "board")
        chips = parse_index_selection(arguments.chips, 4, "chip")
        channels = parse_index_selection(
            arguments.channels, 16, "channel"
        )
        root = settings_root(arguments.frontend_index)

        writes = correction_writes(root) + [
            OdbWrite(
                f"{root}/Sampic Event Collector/mode",
                "default",
                "Use the dedicated vendor-readout worker.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/buffer_size",
                arguments.sampic_buffer_size,
                "Keep enough decoded vendor packets for batched readout.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/sleep_time_us",
                0,
                "Return immediately to the vendor receive path.",
            ),
            OdbWrite(
                f"{root}/Crate/sampling_frequency/frequency_mhz",
                arguments.sampling_frequency_mhz,
                "Set the sampling rate used by acquisition and association.",
            ),
            OdbWrite(
                f"{root}/Crate/sampling_frequency/use_external_clock",
                arguments.external_clock,
                "Select the internal or externally supplied sampling clock.",
            ),
            OdbWrite(
                f"{root}/Crate/frames_per_block",
                arguments.frames_per_block,
                "Use the packetization found reliable in gated-trigger tests.",
            ),
            OdbWrite(
                f"{root}/Crate/triggers_per_event",
                arguments.triggers_per_event,
                "Batch trigger records at the tested maximum.",
            ),
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
                f"{root}/Crate/sync_edge",
                arguments.trigger_edge,
                "Use the same edge convention for external synchronization.",
            ),
            OdbWrite(
                f"{root}/Crate/sync_level",
                arguments.signal_level,
                "Use the same electrical level for external synchronization.",
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
                "external_gated_trigger",
                "Build frontend events around external-trigger timestamps.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/buffer_size",
                arguments.frontend_buffer_size,
                "Buffer the one-event-per-trigger output burst.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/sleep_time_us",
                0,
                "Process decoded packets without an added polling delay.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/hit_time_offset_ns",
                arguments.hit_time_offset_ns,
                "Align hit timestamps with the external trigger.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/pre_window_ns",
                arguments.pre_window_ns,
                "Accept hits this far before the aligned trigger.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/post_window_ns",
                arguments.post_window_ns,
                "Accept hits this far after the aligned trigger.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/sampling_frequency_mhz",
                float(arguments.sampling_frequency_mhz),
                "Convert trigger-cell offsets with the configured sample rate.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/emit_triggers_without_hits",
                True,
                "Keep accepted gates even when no hit is assigned.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/data_bank_enabled",
                not arguments.omit_data_bank,
                "Enable the corrected-hit SD bank.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/advanced_bank_enabled",
                not arguments.omit_advanced_data,
                "Write one compact advanced record for every SD-bank hit.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/advanced_bank_prefix",
                arguments.advanced_bank_prefix,
                "Set the two-character advanced-hit bank prefix.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/event_timing_bank_enabled",
                not arguments.omit_event_timing_bank,
                "Enable the per-event ST timing bank.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/modes/"
                "external_gated_trigger/trigger_metadata_bank_enabled",
                not arguments.omit_trigger_metadata_bank,
                "Enable the SG external-trigger metadata bank.",
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
                writes.extend(
                    [
                        OdbWrite(
                            f"{chip_root}/trigger_option",
                            1,
                            "Enable the SAMPIC channel-trigger option.",
                        ),
                        OdbWrite(
                            f"{chip_root}/central_trigger_mode",
                            0,
                            "OR participating channel primitives.",
                        ),
                        OdbWrite(
                            f"{chip_root}/central_trigger_effect",
                            0,
                            "Read participating channels after the central trigger.",
                        ),
                    ]
                )

                for channel in channels:
                    channel_root = (
                        f"{chip_root}/channels/channel{channel}"
                    )
                    writes.extend(
                        [
                            OdbWrite(
                                f"{channel_root}/enabled",
                                True,
                                "Enable this channel for acquisition.",
                            ),
                            OdbWrite(
                                f"{channel_root}/trigger_mode",
                                0,
                                "Use self-trigger mode for this channel.",
                            ),
                            OdbWrite(
                                f"{channel_root}/trigger_edge",
                                0,
                                "Use the rising edge for channel self-triggering.",
                            ),
                            OdbWrite(
                                f"{channel_root}/pulse_mode",
                                True,
                                "Use positive-pulse self-triggering.",
                            ),
                            OdbWrite(
                                f"{channel_root}/internal_threshold",
                                arguments.threshold_volts,
                                "Set the channel self-trigger threshold.",
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
