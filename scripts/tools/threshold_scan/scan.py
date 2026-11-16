#!/usr/bin/env python3
"""Bounded adaptive threshold response scans; dry-run unless --apply is given."""

from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone
from decimal import Decimal
import fcntl
import importlib.util
import json
import math
from pathlib import Path
import re
import signal
import subprocess
import time
import os
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
EXAMPLE = Path(__file__).with_name("defaults.json")
LIMITED = {"rate_limited", "hit_limited", "byte_limited"}
FIELDS = [
    "feb",
    "physical_path",
    "channel",
    "threshold_v",
    "applied_threshold_v",
    "edge",
    "baseline_v",
    "status",
    "hits",
    "elapsed_s",
    "observed_rate_hz",
    "zero_hit_upper_95_hz",
    "group_size",
    "group_total_hits",
    "group_frame_bytes",
    "attempt",
    "point_id",
    "error",
]


class MeasurementError(RuntimeError):
    def __init__(self, message, *, cleanup_verified=False):
        super().__init__(message)
        self.cleanup_verified = cleanup_verified


class SafetyError(RuntimeError):
    pass


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def resolve(path):
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


def load_config(path):
    config = json.loads(Path(path).read_text())
    return validate_config(config)


def validate_config(config):
    defaults = json.loads(EXAMPLE.read_text())
    unknown = set(config) - set(defaults) - {"name"}
    if unknown:
        raise ValueError(f"Unknown config keys: {sorted(unknown)}")
    defaults.update(config)
    config = defaults
    if config["schema_version"] != 1:
        raise ValueError("Unsupported config schema")
    if set(config["crate"]) != {"ip", "port", "calibration_dir"}:
        raise ValueError("crate needs ip, port and calibration_dir")
    if (
        not isinstance(config["crate"]["ip"], str)
        or not config["crate"]["ip"]
        or len(config["crate"]["ip"]) > 79
    ):
        raise ValueError("Invalid crate IP/hostname")
    if (
        type(config["crate"]["port"]) is not int
        or not 1 <= config["crate"]["port"] <= 65535
    ):
        raise ValueError("Invalid crate port")
    config["crate"]["calibration_dir"] = str(
        resolve(config["crate"]["calibration_dir"])
    )
    if not Path(config["crate"]["calibration_dir"]).is_dir():
        raise ValueError("Calibration directory does not exist")
    if config["edge"] not in ("rising", "falling"):
        raise ValueError("edge must be rising or falling")
    for key in [
        "threshold_min_v",
        "threshold_max_v",
        "threshold_step_v",
        "baseline_v",
        "duration_s",
        "pilot_s",
        "aggregate_rate_limit_hz",
        "retry_delay_s",
        "worker_timeout_s",
        "recovery_timeout_s",
    ]:
        if type(config[key]) not in (float, int) or not math.isfinite(config[key]):
            raise ValueError(f"{key} must be finite")
    if (
        config["threshold_step_v"] <= 0
        or config["threshold_min_v"] > config["threshold_max_v"]
    ):
        raise ValueError("Invalid threshold grid")
    if not 0 <= config["baseline_v"] <= 1.6:
        raise ValueError("baseline_v must be 0..1.6 V")
    if (
        config["baseline_v"] + config["threshold_min_v"] < 0
        or config["baseline_v"] + config["threshold_max_v"] > 1.8
    ):
        raise ValueError("Threshold grid exceeds baseline + DAC range 0..1.8 V")
    if (
        not 0 < config["pilot_s"] <= config["duration_s"] <= 60
        or config["aggregate_rate_limit_hz"] <= 0
    ):
        raise ValueError("Invalid dwell, pilot or rate limit")
    if (
        config["retry_delay_s"] < 0
        or config["worker_timeout_s"] <= config["duration_s"]
        or config["recovery_timeout_s"] <= 0
    ):
        raise ValueError("Invalid retry/watchdog limits")
    for key in [
        "max_hits",
        "max_frame_bytes",
        "max_attempts",
        "recovery_attempts",
        "max_consecutive_failures",
        "sampling_frequency_mhz",
        "adc_bits",
        "samples_per_hit",
    ]:
        if type(config[key]) is not int or config[key] <= 0:
            raise ValueError(f"{key} must be a positive integer")
    if config["sampling_frequency_mhz"] not in (
        1600,
        2133,
        3200,
        4252,
        6400,
        8512,
        10240,
    ):
        raise ValueError("Unsupported internal sampling frequency")
    if not 8 <= config["adc_bits"] <= 11 or not 1 <= config["samples_per_hit"] <= 64:
        raise ValueError("Invalid ADC bits or read length")
    if config["channels"] != "all":
        if not isinstance(config["channels"], list) or not config["channels"]:
            raise ValueError("channels must be all or a nonempty FEB:CHANNEL list")
        if len(set(config["channels"])) != len(config["channels"]):
            raise ValueError("Duplicate channels")
        for key in config["channels"]:
            parse_channel(key)
    thresholds(config)
    return config


def parse_channel(key):
    if not isinstance(key, str) or not re.fullmatch(
        r"[0-3]:(?:[0-9]|[1-5][0-9]|6[0-3])", key
    ):
        raise ValueError(f"Invalid FEB:CHANNEL: {key!r}")
    return tuple(map(int, key.split(":")))


def thresholds(config):
    low, high, step = map(
        lambda k: Decimal(str(config[k])),
        ["threshold_min_v", "threshold_max_v", "threshold_step_v"],
    )
    count = int((high - low) // step) + 1
    if count < 1 or count > 10000:
        raise ValueError("Threshold grid must contain 1..10000 points")
    values = [float(low + i * step) for i in range(count)]
    return sorted(values, reverse=config["edge"] == "rising")


def validate_measurement(result, channels):
    if result.get("schema_version") != 1 or result.get("cleanup_verified") is not True:
        raise SafetyError("Measurement did not confirm source disable/cleanup")
    if result.get("status") not in LIMITED | {"complete"}:
        raise MeasurementError(f'Invalid measurement status: {result.get("status")}')
    live = result.get("elapsed_s")
    if type(live) not in (int, float) or not math.isfinite(live) or live <= 0:
        raise MeasurementError("Invalid live time")
    if set(result.get("counts", {})) != set(channels):
        raise MeasurementError("Missing/unexpected channel counts")
    if any(type(n) is not int or n < 0 for n in result["counts"].values()):
        raise MeasurementError("Invalid hit counts")
    if sum(result["counts"].values()) != result.get("total_hits"):
        raise MeasurementError("Hit counts disagree")
    if set(result.get("applied_thresholds_v", {})) != set(channels):
        raise MeasurementError("Missing applied thresholds")
    if any(
        type(v) not in (int, float) or not math.isfinite(v)
        for v in result["applied_thresholds_v"].values()
    ):
        raise MeasurementError("Invalid applied threshold values")


class WorkerRunner:
    def __init__(self, config, binary):
        self.config, self.binary = config, Path(binary)

    def raw(self, operation, directory, **parameters):
        directory.mkdir(parents=True, exist_ok=False)
        request = {"operation": operation, "config": self.config, **parameters}
        request_path = directory / "request.json"
        result_path = directory / "result.json"
        request_path.write_text(json.dumps(request, indent=2) + "\n")
        timeout = (
            self.config["recovery_timeout_s"]
            if operation == "quiesce"
            else self.config["worker_timeout_s"]
        )
        with (directory / "worker.log").open("w") as log:
            proc = subprocess.Popen(
                [str(self.binary), str(request_path), str(result_path)],
                stdout=log,
                stderr=subprocess.STDOUT,
                cwd=ROOT,
            )
            try:
                code = proc.wait(timeout=timeout)
            except (subprocess.TimeoutExpired, KeyboardInterrupt):
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
                raise
        try:
            result = json.loads(result_path.read_text())
        except (OSError, ValueError) as e:
            raise MeasurementError(
                f"Missing/invalid worker result; see {directory}"
            ) from e
        if code or result.get("status") == "error":
            raise MeasurementError(
                f'{result.get("error",f"worker exited {code}")}; see {directory}',
                cleanup_verified=result.get("cleanup_verified") is True,
            )
        return result

    def __call__(self, operation, directory, **parameters):
        try:
            return self.raw(operation, directory, **parameters)
        except (
            MeasurementError,
            subprocess.TimeoutExpired,
            KeyboardInterrupt,
        ) as error:
            if operation == "measure" and not getattr(error, "cleanup_verified", False):
                print(
                    "Existing worker could not confirm cleanup; waiting before reconnect fallback",
                    flush=True,
                )
                time.sleep(self.config["retry_delay_s"])
                recovered = False
                recovery_error = None
                for attempt in range(1, self.config["recovery_attempts"] + 1):
                    recovery_path = directory / (
                        "recovery" if attempt == 1 else f"recovery_{attempt}"
                    )
                    print(
                        f"[{self.config.get('name', self.config['crate']['ip'])}] Source-disable recovery {attempt}/{self.config['recovery_attempts']}: {recovery_path}",
                        flush=True,
                    )
                    try:
                        result = self.raw("quiesce", recovery_path)
                        if result.get("cleanup_verified") is not True:
                            raise SafetyError("Recovery did not confirm source disable")
                        recovered = True
                        break
                    except (
                        MeasurementError,
                        subprocess.TimeoutExpired,
                        SafetyError,
                    ) as recovery:
                        recovery_error = recovery
                        if attempt < self.config["recovery_attempts"]:
                            time.sleep(
                                min(
                                    self.config["retry_delay_s"] * 2 ** (attempt - 1), 4
                                )
                            )
                if not recovered:
                    raise SafetyError(
                        f"Cannot verify stopped acquisition after {error}; recovery failed: {recovery_error}. Stop/reset crate before proceeding."
                    ) from recovery_error
            if isinstance(error, KeyboardInterrupt):
                raise
            raise MeasurementError(str(error)) from error


class SimulatedRunner:
    """Deterministic demonstration only; no connections or hardware operations."""

    def __init__(self, config):
        self.config = config

    def __call__(self, operation, directory, **parameters):
        directory.mkdir(parents=True, exist_ok=False)
        if operation == "discover":
            return {"schema_version": 1, "board_count": 2, "paths": [0, 3]}
        if operation == "quiesce":
            return {"schema_version": 1, "cleanup_verified": True}
        threshold = parameters["threshold_v"]
        channels = parameters["channels"]
        rates = {}
        for feb, ch in channels:
            key = f"{feb}:{ch}"
            rate = (
                0
                if ch % 11 == 0 or threshold > 0.08 + 0.015 * (ch % 5)
                else 200 * (1 + ch % 7)
            )
            if threshold < 0.035 and ch % 13 == 0:
                rate += 100000
            rates[key] = rate
        total = sum(rates.values())
        limited = total >= self.config["aggregate_rate_limit_hz"]
        live = self.config["pilot_s"] if limited else self.config["duration_s"]
        counts = {k: int(v * live) for k, v in rates.items()}
        result = {
            "schema_version": 1,
            "status": "rate_limited" if limited else "complete",
            "cleanup_verified": True,
            "elapsed_s": live,
            "counts": counts,
            "total_hits": sum(counts.values()),
            "board_paths": [0, 3],
            "frame_bytes": sum(counts.values()) * 148,
            "applied_thresholds_v": {k: threshold for k in rates},
        }
        (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        return result


class Scan:
    def __init__(self, config, runner, output):
        self.config, self.runner, self.output = config, runner, Path(output)
        self.blocked = {}
        self.failures = 0
        self.point_id = 0
        self.writer = None

    def log(self, message):
        line = f"[{timestamp()}] [{self.config.get("name", self.config["crate"]["ip"])}] {message}"
        print(line, flush=True)
        with (self.output / "scan.log").open("a") as f:
            f.write(line + "\n")

    def attempt_group(self, keys, threshold):
        self.point_id += 1
        point = self.point_id
        error = ""
        for attempt in range(1, self.config["max_attempts"] + 1):
            directory = (
                self.output / "attempts" / f"point_{point:06d}_attempt_{attempt}"
            )
            self.log(
                f"{threshold:+.4f} V: {len(keys)} channels, point {point}, attempt {attempt}"
            )
            try:
                self.log(
                    f"  configuring/acquiring; worker details: {directory / 'worker.log'}"
                )
                result = self.runner(
                    "measure",
                    directory,
                    channels=[parse_channel(k) for k in keys],
                    threshold_v=threshold,
                )
                validate_measurement(result, keys)
                if result.get("board_paths") != self.paths:
                    raise SafetyError(
                        "FEB path mapping changed during scan; refusing to mislabel channels"
                    )
                if any(
                    abs(result["applied_thresholds_v"][k] - threshold) > 1e-6
                    for k in keys
                ):
                    raise MeasurementError("Threshold was clamped")
                self.failures = 0
                self.log(
                    f'  {result["status"]}: {result["total_hits"]} hits in {result["elapsed_s"]:.4f}s; {result["total_hits"]/result["elapsed_s"]:,.0f} recorded hits/s'
                )
                rates = [n / result["elapsed_s"] for n in result["counts"].values()]
                self.log(
                    f"  per-channel recorded rates: min={min(rates):.1f}, max={max(rates):.1f} Hz; zero-hit channels={sum(n == 0 for n in result['counts'].values())}; acquisition cleanup confirmed"
                )
                return result, point, attempt
            except MeasurementError as exc:
                error = str(exc)
                self.failures += 1
                with (self.output / "failures.jsonl").open("a") as f:
                    f.write(
                        json.dumps(
                            dict(
                                point_id=point,
                                attempt=attempt,
                                error=error,
                                time=timestamp(),
                            )
                        )
                        + "\n"
                    )
                self.log(f"  failed: {error}")
                if self.failures >= self.config["max_consecutive_failures"]:
                    raise SafetyError(
                        "Consecutive failure budget exhausted; scan aborted"
                    ) from exc
                if attempt < self.config["max_attempts"]:
                    delay = min(self.config["retry_delay_s"] * 2 ** (attempt - 1), 4)
                    self.log(f"  retrying the same group in {delay:g}s after recovery")
                    time.sleep(delay)
        return {"status": "failed", "error": error}, point, attempt

    def row(self, key, threshold, status, result, group, point="", attempt=""):
        feb, channel = parse_channel(key)
        live = result.get("elapsed_s", "")
        hits = result.get("counts", {}).get(key, "")
        row = dict(
            feb=feb,
            physical_path=self.paths[feb],
            channel=channel,
            threshold_v=threshold,
            applied_threshold_v=result.get("applied_thresholds_v", {}).get(key, ""),
            edge=self.config["edge"],
            baseline_v=self.config["baseline_v"],
            status=status,
            hits=hits,
            elapsed_s=live,
            observed_rate_hz=hits / live if live and hits != "" else "",
            zero_hit_upper_95_hz=(
                -math.log(0.05) / live
                if status == "complete" and live and hits == 0
                else ""
            ),
            group_size=group,
            group_total_hits=result.get("total_hits", ""),
            group_frame_bytes=result.get("frame_bytes", ""),
            attempt=attempt,
            point_id=point,
            error=result.get("error", ""),
        )
        self.writer.writerow(row)
        self.stream.flush()

    def group(self, keys, threshold):
        result, point, attempt = self.attempt_group(keys, threshold)
        if result["status"] == "complete":
            for key in keys:
                self.row(key, threshold, "complete", result, len(keys), point, attempt)
        elif result["status"] == "failed":
            for key in keys:
                self.row(key, threshold, "failed", result, len(keys), point, attempt)
            raise SafetyError(
                "Measurement retries exhausted after recovery; stopping crate without splitting channels"
            )
        elif len(keys) > 1:
            mid = len(keys) // 2
            self.log(f"  splitting into groups of {mid} and {len(keys)-mid}")
            self.group(keys[:mid], threshold)
            self.group(keys[mid:], threshold)
        else:
            key = keys[0]
            self.blocked[key] = result["status"]
            self.row(key, threshold, result["status"], result, 1, point, attempt)
            self.log(f'  {key}: stopping more-sensitive points ({result["status"]})')

    def run(self):
        self.output.mkdir(parents=True, exist_ok=False)
        metadata = {
            "schema_version": 1,
            "started": timestamp(),
            "status": "running",
            "simulated": isinstance(self.runner, SimulatedRunner),
        }
        (self.output / "config.json").write_text(
            json.dumps(self.config, indent=2) + "\n"
        )
        try:
            self.log(
                f"Connecting to {self.config['crate']['ip']}:{self.config['crate']['port']}; discovering FEBs"
            )
            discovered = self.runner("discover", self.output / "discovery")
            count = discovered.get("board_count")
            paths = discovered.get("paths")
            if (
                discovered.get("schema_version") != 1
                or type(count) is not int
                or not 1 <= count <= 4
                or not isinstance(paths, list)
                or len(paths) != count
                or any(type(p) is not int or not 0 <= p < 4 for p in paths)
                or len(set(paths)) != count
            ):
                raise SafetyError("Invalid FEB discovery")
            self.paths = paths
            keys = (
                [f"{f}:{c}" for f in range(count) for c in range(64)]
                if self.config["channels"] == "all"
                else self.config["channels"]
            )
            if any(parse_channel(k)[0] >= count for k in keys):
                raise ValueError("Requested FEB absent from discovery")
            self.log(
                f"Discovered {count} FEBs, physical paths {paths}; selected {len(keys)} channels: {', '.join(keys)}"
            )
            metadata["discovery"] = discovered
            metadata["channels"] = keys
            (self.output / "metadata.json").write_text(
                json.dumps(metadata, indent=2) + "\n"
            )
            with (self.output / "results.csv").open("w", newline="") as self.stream:
                self.writer = csv.DictWriter(self.stream, fieldnames=FIELDS)
                self.writer.writeheader()
                grid = thresholds(self.config)
                for index, threshold in enumerate(grid, 1):
                    self.log(
                        f"Threshold {index}/{len(grid)}: {threshold:+.4f} V; {len(self.blocked)} blocked channels"
                    )
                    active = []
                    for key in keys:
                        if key in self.blocked:
                            self.row(
                                key,
                                threshold,
                                "skipped_guard",
                                {"error": self.blocked[key]},
                                0,
                            )
                        else:
                            active.append(key)
                    if active:
                        self.group(active, threshold)
            metadata["status"] = "complete"
            metadata["blocked_channels"] = self.blocked
        except KeyboardInterrupt:
            metadata["status"] = "interrupted"
            raise
        except BaseException as exc:
            metadata["status"] = "failed"
            metadata["error"] = str(exc)
            raise
        finally:
            self.log(f"Scan {metadata['status']}; results: {self.output}")
            metadata["finished"] = timestamp()
            (self.output / "metadata.json").write_text(
                json.dumps(metadata, indent=2) + "\n"
            )
        return metadata


class CrateLock:
    def __init__(self, config):
        self.config = config

    def __enter__(self):
        root = Path.home() / ".cache/sampic-daq/locks"
        root.mkdir(parents=True, exist_ok=True, mode=0o700)
        name = (
            re.sub(r"[^A-Za-z0-9_.-]", "_", self.config["crate"]["ip"])
            + "_"
            + str(self.config["crate"]["port"])
            + ".lock"
        )
        self.file = (root / name).open("a+")
        try:
            fcntl.flock(self.file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            self.file.close()
            raise SafetyError("Another threshold scan owns this endpoint")
        names = subprocess.check_output(["ps", "-eo", "comm="], text=True).splitlines()
        if any(
            n.strip() in ("sampic_frontend", "sampic_deadtime", "sampic_deadtime_")
            for n in names
        ):
            self.file.close()
            raise SafetyError("Stop local SAMPIC frontends/diagnostics first")
        return self

    def __exit__(self, *_):
        self.file.close()


def load_configs(path):
    raw = json.loads(Path(path).read_text())
    if "crates" not in raw:
        return [validate_config(raw)]
    shared = {k: v for k, v in raw.items() if k != "crates"}
    if not isinstance(raw["crates"], list) or not raw["crates"]:
        raise ValueError("crates must be a nonempty list")
    configs = []
    names, endpoints = set(), set()
    for entry in raw["crates"]:
        if not isinstance(entry, dict):
            raise ValueError("Each crate must be an object")
        config = validate_config({**shared, **entry})
        name = config.get("name")
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_-]+", name):
            raise ValueError(
                "Each crate needs a name containing letters, numbers, _ or -"
            )
        endpoint = (config["crate"]["ip"], config["crate"]["port"])
        if name in names or endpoint in endpoints:
            raise ValueError("Duplicate crate name or endpoint")
        names.add(name)
        endpoints.add(endpoint)
        configs.append(config)
    return configs


def launch_screen(args):
    if not args.apply and not args.simulate:
        raise ValueError("Use --screen with --apply or --simulate")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.screen):
        raise ValueError("Invalid screen session name")
    if not shutil.which("screen"):
        raise ValueError("GNU screen is not installed")
    sessions = subprocess.run(["screen", "-ls"], capture_output=True, text=True)
    if re.search(r"\d+\." + re.escape(args.screen) + r"\s", sessions.stdout):
        raise ValueError(
            f"Screen session already exists; attach with screen -r {args.screen}"
        )
    command = [sys.executable, "-u", str(Path(__file__).resolve())]
    for flag, value in [
        ("--config", args.config.resolve()),
        ("--worker", args.worker.resolve()),
    ]:
        command += [flag, str(value)]
    if args.output_dir:
        command += ["--output-dir", str(resolve(args.output_dir))]
    command += ["--apply" if args.apply else "--simulate"]
    if args.no_plots:
        command.append("--no-plots")
    # Run directly: screen exits when the scanner exits; persistent logs remain.
    subprocess.run(
        ["screen", "-dmS", args.screen, *command],
        cwd=ROOT,
        check=True,
    )
    print(
        f"Started screen session {args.screen}. Attach: screen -r {args.screen}",
        flush=True,
    )
    print(
        "Ctrl-A D detaches and keeps scanning; Ctrl-C while attached stops all crates and cleans up.",
        flush=True,
    )


def stop_children(children, timeout):
    print("Stopping all crate scans; waiting for source-disable cleanup…", flush=True)
    # A second Ctrl-C must not interrupt in-flight recovery.
    previous = {
        sig: signal.signal(sig, signal.SIG_IGN)
        for sig in (signal.SIGINT, signal.SIGTERM)
    }
    try:
        for _, child in children:
            if child.poll() is None:
                child.send_signal(signal.SIGTERM)
        deadline = time.monotonic() + timeout
        for name, child in children:
            try:
                child.wait(timeout=max(0.01, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
                print(
                    f"[{name}] Cleanup timed out; hardware stop is UNVERIFIED. Stop/reset this crate manually.",
                    flush=True,
                )
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)


def run_crates(args, configs):
    for config in configs:
        crate = config["crate"]
        print(
            f"Crate {config.get('name', crate['ip'])}: {crate['ip']}:{crate['port']}; channels={config['channels']}; "
            f"{len(thresholds(config))} thresholds; calibration={crate['calibration_dir']}",
            flush=True,
        )
    print(
        f"{len(configs)} crate(s) will scan concurrently, with independent guards and retries.",
        flush=True,
    )
    if not args.apply and not args.simulate:
        print(
            "Dry run: no hardware connections. Add --apply, optionally --screen threshold_scan."
        )
        return
    if args.apply and not args.worker.is_file():
        raise ValueError(
            "Build first: cmake --build build --target sampic_threshold_scan_worker -j 8"
        )
    stamp = ("SIMULATED_" if args.simulate else "") + datetime.now().strftime(
        "%Y%m%d_%H%M%S_%f"
    )
    output = (
        resolve(args.output_dir)
        if args.output_dir
        else resolve(configs[0]["output_root"]) / stamp
    )
    output.mkdir(parents=True, exist_ok=False)
    (output / "configs").mkdir()
    print(f"Run output: {output}", flush=True)
    children = []
    metadata = {"started": timestamp(), "status": "running", "crates": {}}
    try:
        for index, config in enumerate(configs):
            name = config.get("name", f"crate_{index+1}")
            config["name"] = name
            path = output / "configs" / f"{name}.json"
            path.write_text(json.dumps(config, indent=2) + "\n")
            command = [
                sys.executable,
                "-u",
                str(Path(__file__).resolve()),
                "--single",
                "--config",
                str(path),
                "--output-dir",
                str(output / name),
                "--worker",
                str(args.worker.resolve()),
                "--simulate" if args.simulate else "--apply",
            ]
            command.append("--no-plots")
            child = subprocess.Popen(command, cwd=ROOT, start_new_session=True)
            children.append((name, child))
            metadata["crates"][name] = {
                "endpoint": config["crate"],
                "output": str(output / name),
            }
        while any(child.poll() is None for _, child in children):
            failed = [
                (name, child.returncode)
                for name, child in children
                if child.poll() not in (None, 0)
            ]
            if failed:
                raise SafetyError(
                    f"Crate scan failed: {failed}; stopping remaining crates"
                )
            time.sleep(0.1)
        failed = [
            (name, child.returncode) for name, child in children if child.returncode
        ]
        if failed:
            raise SafetyError(f"Crate scan failed: {failed}")
        metadata["status"] = "complete"
    except BaseException as error:
        metadata["status"] = (
            "interrupted" if isinstance(error, KeyboardInterrupt) else "failed"
        )
        metadata["error"] = str(error)
        stop_children(
            children,
            max(
                c["worker_timeout_s"]
                + c["recovery_timeout_s"] * c["recovery_attempts"]
                + 4 * c["recovery_attempts"]
                + 10
                for c in configs
            ),
        )
        raise
    finally:
        metadata["finished"] = timestamp()
        for name, child in children:
            metadata["crates"][name]["exit_code"] = child.poll()
        (output / "run.json").write_text(json.dumps(metadata, indent=2) + "\n")
        print(
            f"Multi-crate scan {metadata['status']}; partial/full results: {output}",
            flush=True,
        )
    if not args.no_plots:
        if importlib.util.find_spec("matplotlib"):
            plot_results(output)
        else:
            print(
                f"Plotting needs matplotlib/numpy. Plot later: python3 {Path(__file__)} --plot {output}"
            )


def plot_results(directory):
    from plots import plot

    directory = Path(directory)
    if (directory / "run.json").is_file():
        run = json.loads((directory / "run.json").read_text())
        for name in run["crates"]:
            try:
                plot(directory / name)
            except Exception as error:
                print(
                    f"[{name}] Plotting failed; acquisition data preserved: {error}",
                    flush=True,
                )
    else:
        plot(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        type=Path,
        default=Path(__file__).with_name("config.example.json"),
    )
    parser.add_argument(
        "--screen",
        metavar="SESSION",
        help="Launch detached GNU screen session; attach with screen -r SESSION",
    )
    parser.add_argument("--single", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument(
        "--apply", action="store_true", help="Run on hardware; otherwise plan only"
    )
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="Generate labeled synthetic data; no hardware",
    )
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument(
        "--no-plots",
        action="store_true",
        help="Keep acquisition results without generating plots",
    )
    parser.add_argument(
        "--worker", type=Path, default=ROOT / "build/bin/sampic_threshold_scan_worker"
    )
    parser.add_argument(
        "--plot", type=Path, help="Generate plots for an existing results directory"
    )
    args = parser.parse_args()
    if args.plot:
        plot_results(args.plot)
        return
    if args.apply and args.simulate:
        parser.error("--apply and --simulate are mutually exclusive")
    configs = load_configs(args.config)
    if args.screen:
        for config in configs:
            crate = config["crate"]
            print(
                f"Crate {config.get('name', crate['ip'])}: {crate['ip']}:{crate['port']}; channels={config['channels']}",
                flush=True,
            )
        launch_screen(args)
        return
    if not args.single:
        run_crates(args, configs)
        return
    config = configs[0]
    print(
        f"Crate {config.get('name', 'unnamed')}: {config['crate']['ip']}:{config['crate']['port']}",
        flush=True,
    )
    grid = thresholds(config)
    print(
        f'Threshold scan: {len(grid)} points, {grid[0]:+.4f} to {grid[-1]:+.4f} V relative to baseline ({config["edge"]})'
    )
    print(
        f'Channels: {config["channels"]}; begin together, split only on guard/failure. Duration {config["duration_s"]} s/group.'
    )
    print(
        "Recorded rates only; no assumed good rate, automatic threshold recommendation, or ODB writes."
    )
    if not args.apply and not args.simulate:
        print(
            "Dry run: no connections or hardware changes. Use --apply to run, or --simulate for a demonstration."
        )
        return
    stamp = ("SIMULATED_" if args.simulate else "") + datetime.now().strftime(
        "%Y%m%d_%H%M%S_%f"
    )
    output = (
        resolve(args.output_dir)
        if args.output_dir
        else resolve(config["output_root"]) / stamp
    )
    if args.simulate:
        Scan(config, SimulatedRunner(config), output).run()
    else:
        if not args.worker.is_file():
            raise ValueError(
                f"Build the worker first: cmake --build build --target sampic_threshold_scan_worker"
            )
        with CrateLock(config):
            Scan(config, WorkerRunner(config, args.worker.resolve()), output).run()
    print(f"Results: {output}")
    if not args.no_plots and importlib.util.find_spec("matplotlib"):
        from plots import plot

        try:
            plot(output)
        except Exception as exc:
            print(f"Plotting failed; acquisition data is preserved: {exc}")
    elif not args.no_plots:
        print("Plotting needs matplotlib; acquisition data is preserved.")
    print(f"Plot again with: python3 {Path(__file__)} --plot {output}")


if __name__ == "__main__":

    def cancel(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, cancel)
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit(
            "Interrupted; partial results retained and in-flight acquisition recovery attempted."
        )
    except Exception as exc:
        raise SystemExit(f"Threshold scan stopped: {exc}") from exc
