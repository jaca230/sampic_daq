"""One-to-one vendor-event ODB profile."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import (
    OdbProfile,
    OdbWrite,
    correction_writes,
    settings_root,
)


class VendorPassthroughProfile(OdbProfile):
    """Preserve decoded SAMPIC event boundaries without time grouping."""

    name = "vendor_passthrough"
    description = (
        "Map each decoded vendor event to one MIDAS event without time grouping."
    )

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("--sampic-buffer-size", type=int, default=512)
        parser.add_argument("--frontend-buffer-size", type=int, default=512)
        parser.add_argument("--wait-timeout-ms", type=int, default=1000)
        parser.add_argument("--omit-data-bank", action="store_true")
        parser.add_argument(
            "--omit-trigger-records",
            action="store_true",
            help="Do not write the vendor trigger-record bank.",
        )
        parser.add_argument(
            "--omit-advanced-data",
            action="store_true",
            help="Do not write the compact advanced-hit bank.",
        )
        parser.add_argument("--advanced-bank-prefix", default="SH")
        parser.add_argument("--omit-event-timing-bank", action="store_true")

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if (
            arguments.sampic_buffer_size <= 0
            or arguments.frontend_buffer_size <= 0
        ):
            raise ValueError("buffer sizes must be positive")
        if arguments.wait_timeout_ms <= 0:
            raise ValueError("wait timeout must be positive")
        if len(arguments.advanced_bank_prefix) != 2:
            raise ValueError("advanced bank prefix must contain exactly 2 characters")
        if not arguments.omit_advanced_data and arguments.advanced_bank_prefix == "SD":
            raise ValueError("advanced and data bank prefixes must differ")
        root = settings_root(arguments.frontend_index)
        mode_root = f"{root}/Frontend Event Collector/modes/vendor_passthrough"
        return correction_writes(root) + [
            OdbWrite(
                f"{root}/Sampic Event Collector/mode",
                "default",
                "Use standard vendor event decoding.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/buffer_size",
                arguments.sampic_buffer_size,
                "Buffer decoded vendor events before MIDAS packaging.",
            ),
            OdbWrite(
                f"{root}/Sampic Event Collector/sleep_time_us",
                0,
                "Return immediately to vendor readout.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/mode",
                "vendor_passthrough",
                "Disable time grouping and preserve vendor event boundaries.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/buffer_size",
                arguments.frontend_buffer_size,
                "Buffer MIDAS-ready pass-through events.",
            ),
            OdbWrite(
                f"{root}/Frontend Event Collector/sleep_time_us",
                0,
                "Package decoded events without an added polling delay.",
            ),
            OdbWrite(
                f"{mode_root}/wait_timeout_ms",
                arguments.wait_timeout_ms,
                "Bound idle waits while retaining blocking readout.",
            ),
            OdbWrite(
                f"{mode_root}/trigger_bank_enabled",
                not arguments.omit_trigger_records,
                "Retain the vendor packet's trigger-record array.",
            ),
            OdbWrite(
                f"{mode_root}/data_bank_enabled",
                not arguments.omit_data_bank,
                "Enable the corrected-hit SD bank.",
            ),
            OdbWrite(
                f"{mode_root}/advanced_bank_enabled",
                not arguments.omit_advanced_data,
                "Write one compact advanced record for every SD-bank hit.",
            ),
            OdbWrite(
                f"{mode_root}/advanced_bank_prefix",
                arguments.advanced_bank_prefix,
                "Set the two-character advanced-hit bank prefix.",
            ),
            OdbWrite(
                f"{mode_root}/event_timing_bank_enabled",
                not arguments.omit_event_timing_bank,
                "Enable the per-event ST timing bank.",
            ),
        ]


PROFILE = VendorPassthroughProfile()
