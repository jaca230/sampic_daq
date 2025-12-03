#!/usr/bin/env python3
"""
Headless controller for the LeCroy 9210 pulse generator.

This utility mirrors the GUI workflow that lives in /home/vincentp/lecroy by:
  * Loading the same YAML configuration format (sections G/A/B).
  * Sending the resulting SCPI commands over a raw socket.
  * Optionally toggling the channel outputs, issuing manual triggers,
    and printing a readback of the most important settings.

Typical usage:
  ./headless_cli.py --config /home/vincentp/lecroy/lecroy_config.yaml --readback
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path
from typing import Dict, Iterable, Optional

try:
    import yaml  # type: ignore
except ImportError:  # pragma: no cover - PyYAML should exist on the bench machine.
    yaml = None


def _format_value(value) -> str:
    if isinstance(value, bool):
        return "ON" if value else "OFF"
    if isinstance(value, (int,)) and not isinstance(value, bool):
        return str(value)
    if isinstance(value, float):
        magnitude = abs(value)
        if magnitude != 0.0 and (magnitude < 1e-3 or magnitude >= 1e4):
            return f"{value:.6E}"
        return f"{value:.9f}".rstrip("0").rstrip(".")
    return str(value)


class LecroySocket:
    def __init__(self, ip: str, port: int, timeout: float, delay: float):
        self._sock = socket.create_connection((ip, port), timeout=timeout)
        self._sock.settimeout(timeout)
        self._delay = max(0.0, delay)

    def close(self) -> None:
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._sock.close()

    def _send_line(self, line: str) -> None:
        payload = (line.rstrip("\n") + "\n").encode("ascii")
        self._sock.sendall(payload)
        if self._delay:
            time.sleep(self._delay)

    def query(self, command: str) -> str:
        cmd = command if command.endswith("?") else f"{command}?"
        self._send_line(cmd)
        data = bytearray()
        while True:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise RuntimeError("connection closed while waiting for response")
            data.extend(chunk)
            if b"\n" in chunk:
                break
        return data.decode(errors="replace").strip()

    def write(self, command: str, value: Optional[str] = None) -> None:
        if value is not None:
            self._send_line(f"{command} {value}")
        else:
            self._send_line(command)

    def apply_config(self, config: Dict[str, Dict[str, object]]) -> None:
        for key, value in config.get("G", {}).items():
            self.write(key, _format_value(value))
        for channel in ("A", "B"):
            channel_cfg = config.get(channel)
            if not channel_cfg:
                continue
            for key, value in channel_cfg.items():
                self.write(f"{channel}:{key}", _format_value(value))


def load_config(path: Path) -> Dict[str, Dict[str, object]]:
    with path.open("r", encoding="utf-8") as handle:
        if path.suffix.lower() in {".yaml", ".yml"}:
            if yaml is None:
                raise RuntimeError("PyYAML is required to load YAML configuration files")
            return yaml.safe_load(handle)
        return json.load(handle)


def _safe_query(device: LecroySocket, command: str) -> str:
    try:
        return device.query(command)
    except Exception as exc:  # pragma: no cover - hardware errors depend on bench state.
        return f"<error: {exc}>"


def _print_readback(device: LecroySocket, channels: Iterable[str]) -> None:
    print("Readback summary:")
    print(f"  FREQ?       = {_safe_query(device, 'FREQ?')}")
    for channel in channels:
        chan = channel.upper()
        for field in ("DBL?", "AMP?", "BASE?", "WID?", "DEL?", "DISA?"):
            response = _safe_query(device, f"{chan}:{field}")
            print(f"  {chan}:{field:<5} = {response}")


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Headless interface for the LeCroy 9210.")
    parser.add_argument("--ip", default="10.0.1.103", help="Generator IP (default: 10.0.1.103)")
    parser.add_argument("--port", type=int, default=1234, help="Generator TCP port (default: 1234)")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout in seconds")
    parser.add_argument("--delay", type=float, default=0.05, help="Wait time between SCPI commands.")
    parser.add_argument("--config", type=Path, help="YAML/JSON file with G/A/B sections to apply.")
    parser.add_argument("--readback", action="store_true", help="Print key parameters after setup.")
    parser.add_argument("--dump-lrn", action="store_true", help="Print the raw *LRN? result.")
    parser.add_argument("--enable-output", action="append", default=[],
                        help="Enable (DISA OFF) for the given channel. Can be repeated.")
    parser.add_argument("--disable-output", action="append", default=[],
                        help="Disable (DISA ON) for the given channel. Can be repeated.")
    parser.add_argument("--trigger", action="store_true", help="Issue a *TRG manual trigger.")
    parser.add_argument("--command", action="append", default=[],
                        help="Extra SCPI command(s) to send without reading.")
    parser.add_argument("--query", action="append", default=[],
                        help="Additional query command(s) to print the response.")
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv)
    cfg: Optional[Dict[str, Dict[str, object]]] = None
    if args.config:
        try:
            cfg = load_config(args.config)
        except Exception as exc:
            print(f"Failed to load configuration {args.config}: {exc}", file=sys.stderr)
            return 2

    try:
        device = LecroySocket(args.ip, args.port, args.timeout, args.delay)
    except OSError as exc:
        print(f"Unable to connect to {args.ip}:{args.port}: {exc}", file=sys.stderr)
        return 1

    try:
        ident = _safe_query(device, "*IDN?")
        print(f"Instrument ID: {ident}")
        if "Agilent" in ident and "33250A" in ident:
            print("NOTE: This looks like the Agilent 33250A, not the LeCroy 9210.", file=sys.stderr)

        if cfg:
            device.apply_config(cfg)
            print("Configuration applied.")

        for ch in args.enable_output:
            device.write(f"{ch.upper()}:DISA", "OFF")
        for ch in args.disable_output:
            device.write(f"{ch.upper()}:DISA", "ON")

        if args.trigger:
            device.write("*TRG")

        for cmd in args.command:
            device.write(cmd)

        for cmd in args.query:
            print(f"{cmd}? -> {_safe_query(device, cmd)}")

        if args.dump_lrn:
            print(f"*LRN? -> {_safe_query(device, '*LRN?')}")

        if args.readback:
            channels = []
            if cfg:
                channels.extend(cfg.keys())
            if not channels:
                channels = ["A", "B"]
            interesting = [c for c in channels if c.upper() in {"A", "B"}]
            if not interesting:
                interesting = ["A"]
            _print_readback(device, interesting)
    finally:
        device.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
