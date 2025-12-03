#!/usr/bin/env python3
"""
Print the current Lecroy 9210 configuration without applying any changes.
"""

from __future__ import annotations

import argparse
import sys

import headless_cli


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Read back Lecroy 9210 settings.")
    parser.add_argument("--ip", default="10.0.1.103", help="Generator IP (default 10.0.1.103).")
    parser.add_argument("--port", type=int, default=1234, help="Generator TCP port (default 1234).")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout seconds.")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between SCPI commands.")
    parser.add_argument("--channels", nargs="*", default=["A", "B"],
                        help="Channel letters to query (default: A B).")
    parser.add_argument("--dump-lrn", action="store_true",
                        help="Print the raw *LRN? response in addition to summary.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        device = headless_cli.LecroySocket(args.ip, args.port, args.timeout, args.delay)
    except OSError as exc:
        print(f"Unable to connect to {args.ip}:{args.port}: {exc}", file=sys.stderr)
        return 1

    try:
        ident = headless_cli._safe_query(device, "*IDN?")
        print(f"Instrument ID: {ident}")
        if args.dump_lrn:
            lrn = headless_cli._safe_query(device, "*LRN?")
            print("*LRN? response:")
            print(lrn)
        headless_cli._print_readback(device, args.channels)
    finally:
        device.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
