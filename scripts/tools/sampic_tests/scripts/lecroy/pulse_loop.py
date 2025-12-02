#!/usr/bin/env python3
"""
Continuously trigger the LeCroy 9210 using the same SCPI interface as headless_cli.py.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import headless_cli


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Looping double-pulse trigger helper.")
    parser.add_argument("--config", type=Path,
                        help="YAML/JSON Lecroy config (same format as headless_cli).")
    parser.add_argument("--ip", default="10.0.1.103", help="Generator IP (default 10.0.1.103).")
    parser.add_argument("--port", type=int, default=1234, help="Generator TCP port.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout seconds.")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between SCPI commands.")
    parser.add_argument("--interval", type=float, default=0.01,
                        help="Delay between triggers in seconds (default 0.01).")
    parser.add_argument("--count", type=int, default=0,
                        help="Number of triggers to send (0 = infinite).")
    parser.add_argument("--apply-config", action="store_true",
                        help="Apply the YAML config before pulsing (default: only trigger).")
    parser.add_argument("--readback", action="store_true",
                        help="Print the configuration readback before looping.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = None
    if args.config:
        try:
            config = headless_cli.load_config(args.config)
        except Exception as exc:
            print(f"Failed to load config {args.config}: {exc}", file=sys.stderr)
            return 2

    device = headless_cli.LecroySocket(args.ip, args.port, args.timeout, args.delay)
    try:
        ident = headless_cli._safe_query(device, "*IDN?")
        print(f"Instrument ID: {ident}")
        if args.apply_config:
            if config is None:
                print("Apply-config requested but no config file provided.", file=sys.stderr)
                return 2
            device.apply_config(config)
            print("Configuration applied.")
        if args.readback:
            headless_cli._print_readback(device, ["A", "B"])

        sent = 0
        print("Starting trigger loop (Ctrl+C to stop)...")
        while args.count <= 0 or sent < args.count:
            device.write("*TRG")
            sent += 1
            if args.interval > 0.0:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        device.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
