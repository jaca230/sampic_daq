#!/usr/bin/env python3
"""
Quick LeCroy 9210 helper that applies ad-hoc parameter overrides and prints a readback
without requiring a config file.
"""

from __future__ import annotations

import argparse
import sys
from typing import Dict, List, Tuple

import headless_cli


def _format_seconds_from_ns(value_ns: float) -> str:
    return headless_cli._format_value(value_ns * 1e-9)


def _bool_to_on_off(value: bool) -> str:
    return "ON" if value else "OFF"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Apply quick LeCroy 9210 parameter tweaks without a config file."
    )
    parser.add_argument("--ip", default="10.0.1.103", help="Generator IP (default 10.0.1.103).")
    parser.add_argument("--port", type=int, default=1234, help="Generator TCP port.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout seconds.")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between SCPI commands.")
    parser.add_argument(
        "--channel",
        default="A",
        choices=["A", "B"],
        help="Channel to control/read back (default A).",
    )
    parser.add_argument("--frequency-hz", type=float, help="Set generator repetition frequency.")
    parser.add_argument("--trigger-mode", choices=["NORMAL", "SINGLE", "BURST"], help="Set TRMD.")
    parser.add_argument("--trigger-slope", choices=["POS", "NEG"], help="Set TRSL.")
    parser.add_argument("--trigger-level-v", type=float, help="Set TRLV (volts).")
    parser.add_argument("--amplitude-v", type=float, help="Channel amplitude in volts.")
    parser.add_argument("--baseline-v", type=float, help="Channel baseline in volts.")
    parser.add_argument("--width-ns", type=float, help="Pulse width in nanoseconds.")
    parser.add_argument("--lead-ns", type=float, help="Leading edge time in nanoseconds.")
    parser.add_argument("--trail-ns", type=float, help="Trailing edge time in nanoseconds.")
    parser.add_argument("--double-delay-ns", type=float, help="Double-pulse delay in nanoseconds.")
    parser.add_argument(
        "--double-pulse",
        choices=["on", "off"],
        help="Enable or disable double-pulse mode for the channel.",
    )
    parser.add_argument(
        "--output-main",
        choices=["on", "off"],
        help="Toggle the main output (OUT) for the channel.",
    )
    parser.add_argument(
        "--output-inverse",
        choices=["on", "off"],
        help="Toggle the inverted output (OUTB) for the channel.",
    )
    parser.add_argument(
        "--disable-channel",
        choices=["on", "off"],
        help="Set DISA state (ON disables the channel output).",
    )
    parser.add_argument(
        "--trigger",
        action="store_true",
        help="Issue a manual trigger (*TRG) after applying updates.",
    )
    parser.add_argument(
        "--readback-only",
        action="store_true",
        help="Skip parameter updates and only print the readback.",
    )
    return parser.parse_args()


def build_updates(args: argparse.Namespace) -> List[Tuple[str, str]]:
    updates: List[Tuple[str, str]] = []
    channel = args.channel.upper()
    prefix = f"{channel}:"

    if args.frequency_hz is not None:
        updates.append(("FREQ", headless_cli._format_value(args.frequency_hz)))
    if args.trigger_mode:
        updates.append(("TRMD", args.trigger_mode))
    if args.trigger_slope:
        updates.append(("TRSL", args.trigger_slope))
    if args.trigger_level_v is not None:
        updates.append(("TRLV", headless_cli._format_value(args.trigger_level_v)))
    if args.amplitude_v is not None:
        updates.append((prefix + "AMP", headless_cli._format_value(args.amplitude_v)))
    if args.baseline_v is not None:
        updates.append((prefix + "BASE", headless_cli._format_value(args.baseline_v)))
    if args.width_ns is not None:
        updates.append((prefix + "WID", _format_seconds_from_ns(args.width_ns)))
    if args.lead_ns is not None:
        updates.append((prefix + "LEAD", _format_seconds_from_ns(args.lead_ns)))
    if args.trail_ns is not None:
        updates.append((prefix + "TRAIL", _format_seconds_from_ns(args.trail_ns)))
    if args.double_delay_ns is not None:
        updates.append((prefix + "DEL", _format_seconds_from_ns(args.double_delay_ns)))
    if args.double_pulse:
        updates.append((prefix + "DBL", _bool_to_on_off(args.double_pulse == "on")))
    if args.output_main:
        updates.append((prefix + "OUT", _bool_to_on_off(args.output_main == "on")))
    if args.output_inverse:
        updates.append((prefix + "OUTB", _bool_to_on_off(args.output_inverse == "on")))
    if args.disable_channel:
        updates.append((prefix + "DISA", _bool_to_on_off(args.disable_channel == "on")))
    return updates


def apply_updates(device: headless_cli.LecroySocket, updates: List[Tuple[str, str]]) -> None:
    if not updates:
        return
    print("Applying parameter updates:")
    for command, value in updates:
        print(f"  {command} {value}")
        device.write(command, value)


def main() -> int:
    args = parse_args()
    try:
        device = headless_cli.LecroySocket(args.ip, args.port, args.timeout, args.delay)
    except OSError as exc:
        print(f"Unable to connect to {args.ip}:{args.port}: {exc}", file=sys.stderr)
        return 1

    updates = [] if args.readback_only else build_updates(args)

    try:
        ident = headless_cli._safe_query(device, "*IDN?")
        print(f"Instrument ID: {ident}")
        apply_updates(device, updates)
        if args.trigger:
            device.write("*TRG")
            print("Manual trigger issued.")
        print("Readback:")
        headless_cli._print_readback(device, [args.channel.upper()])
    finally:
        device.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
