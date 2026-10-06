"""Hit-time-grouped frontend collector ODB profile."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import (
    OdbProfile,
    OdbWrite,
    correction_writes,
    settings_root,
)


class TimeGroupedProfile(OdbProfile):
    """Select the legacy/default hit-time clustering strategy."""

    name = "time_grouped"
    description = "Group SAMPIC hits into MIDAS events by timestamp proximity."

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("--time-window-ns", type=float, default=1_000_000.0)
        parser.add_argument("--finalize-after-ms", type=float, default=10.0)
        parser.add_argument("--wait-timeout-ms", type=int, default=1000)
        parser.add_argument("--frames-per-block", type=int, default=31)
        parser.add_argument("--sampic-buffer-size", type=int, default=512)
        parser.add_argument("--frontend-buffer-size", type=int, default=512)
        parser.add_argument("--frontend-sleep-time-us", type=int, default=0)

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if arguments.time_window_ns < 0 or arguments.finalize_after_ms < 0:
            raise ValueError("grouping windows must be non-negative")
        if arguments.wait_timeout_ms <= 0:
            raise ValueError("wait timeout must be positive")
        if arguments.frames_per_block < 1 or arguments.frames_per_block > 31:
            raise ValueError("frames per block must be in [1, 31]")
        if arguments.frontend_sleep_time_us < 0:
            raise ValueError("frontend sleep time must be non-negative")
        if (
            arguments.sampic_buffer_size <= 0
            or arguments.frontend_buffer_size <= 0
        ):
            raise ValueError("buffer sizes must be positive")
        root = settings_root(arguments.frontend_index)
        mode_root = f"{root}/Frontend Event Collector/modes/default"
        return correction_writes(root) + [
            OdbWrite(
                f"{root}/Sampic Event Collector/mode",
                "default",
                "Use standard vendor event decoding.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/buffer_size",
                arguments.sampic_buffer_size,
                "Buffer decoded vendor events before grouping.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/sleep_time_us",
                0,
                "Return immediately to vendor readout.",
            ),
            OdbWrite(
                f"{root}/Crate/frames_per_block",
                arguments.frames_per_block,
                "Batch vendor frames to reduce transport and decode overhead.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/mode",
                "default",
                "Enable hit-time clustering.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/buffer_size",
                arguments.frontend_buffer_size,
                "Buffer grouped MIDAS-ready events.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/sleep_time_us",
                arguments.frontend_sleep_time_us,
                "Avoid an added delay between grouping passes.",
            ),
            OdbWrite(
                f"{mode_root}/time_window_ns",
                arguments.time_window_ns,
                "Group hits within this timestamp window.",
            ),
            OdbWrite(
                f"{mode_root}/finalize_after_ms",
                arguments.finalize_after_ms,
                "Finalize an inactive pending group after this delay.",
            ),
            OdbWrite(
                f"{mode_root}/wait_timeout_ms",
                arguments.wait_timeout_ms,
                "Bound idle waits for decoded events.",
            ),
        ]


PROFILE = TimeGroupedProfile()
