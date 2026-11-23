"""Ordered, pre-correction SAMPIC waveform profile for event-builder tests."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import OdbProfile, OdbWrite, settings_root


class VendorRawPassthroughProfile(OdbProfile):
    name = "vendor_raw_passthrough"
    description = (
        "Preserve vendor-event boundaries and publish ordered integer ADC samples."
    )

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("--sampic-buffer-size", type=int, default=512)
        parser.add_argument("--frontend-buffer-size", type=int, default=512)
        parser.add_argument("--wait-timeout-ms", type=int, default=1000)
        parser.add_argument("--raw-bank-prefix", default="SR")
        parser.add_argument("--omit-trigger-records", action="store_true")
        parser.add_argument("--omit-acquisition-timing-bank", action="store_true")

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if arguments.sampic_buffer_size <= 0 or arguments.frontend_buffer_size <= 0:
            raise ValueError("buffer sizes must be positive")
        if arguments.wait_timeout_ms <= 0:
            raise ValueError("wait timeout must be positive")
        if len(arguments.raw_bank_prefix) != 2:
            raise ValueError("raw bank prefix must contain exactly 2 characters")

        root = settings_root(arguments.frontend_index)
        mode_root = f"{root}/Frontend Event Collector/modes/vendor_raw_passthrough"
        return [
            OdbWrite(f"{root}/Sampic Event Collector/mode", "default",
                     "Use standard vendor frame acquisition and decoding."),
            OdbWrite(f"{root}/Sampic Event Collector/buffer_size",
                     arguments.sampic_buffer_size, "Buffer decoded vendor events."),
            OdbWrite(f"{root}/Sampic Event Collector/sleep_time_us", 0,
                     "Do not add a polling delay to readout."),
            OdbWrite(f"{root}/Frontend Event Collector/mode",
                     "vendor_raw_passthrough",
                     "Publish time-ordered, pre-correction integer samples."),
            OdbWrite(f"{root}/Frontend Event Collector/buffer_size",
                     arguments.frontend_buffer_size,
                     "Buffer MIDAS-ready raw events."),
            OdbWrite(f"{root}/Frontend Event Collector/sleep_time_us", 0,
                     "Do not add a packaging delay."),
            OdbWrite(f"{mode_root}/wait_timeout_ms", arguments.wait_timeout_ms,
                     "Bound idle blocking waits."),
            OdbWrite(f"{mode_root}/raw_data_bank_enabled", True,
                     "Enable the ordered raw waveform bank."),
            OdbWrite(f"{mode_root}/raw_data_bank_prefix",
                     arguments.raw_bank_prefix, "Set the ordered raw bank prefix."),
            OdbWrite(f"{mode_root}/trigger_bank_enabled",
                     not arguments.omit_trigger_records,
                     "Retain vendor trigger records."),
            OdbWrite(f"{mode_root}/acquisition_timing_bank_prefix", "SA",
                     "Use the event-builder acquisition timing contract."),
            OdbWrite(f"{mode_root}/acquisition_timing_bank_enabled",
                     not arguments.omit_acquisition_timing_bank,
                     "Retain frontend timing metadata."),
        ]


PROFILE = VendorRawPassthroughProfile()
