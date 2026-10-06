#!/usr/bin/env python3
"""Find the highest Agilent rate reproduced by a running MIDAS frontend.

The generator frequency is varied while the script samples the standard MIDAS
equipment statistics. A point passes when the median reported event rate is
within the requested fractional tolerance of the expected event rate.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[5]
LECROY_HELPERS = Path(__file__).resolve().parent / "lecroy"
ODB_TOOLS = REPO_ROOT / "scripts" / "odb_tools"
for module_path in (LECROY_HELPERS, ODB_TOOLS):
    if str(module_path) not in sys.path:
        sys.path.insert(0, str(module_path))

import headless_cli  # noqa: E402
from midas_client_utils import create_midas_client  # noqa: E402


@dataclass
class RateSample:
    elapsed_s: float
    timestamp_utc: str
    events_sent: int
    reported_rate_hz: float
    reported_kbytes_per_s: float


@dataclass
class TrialResult:
    sequence: int
    requested_rate_hz: int
    generator_readback_hz: float
    expected_event_rate_hz: float
    lower_limit_hz: float
    upper_limit_hz: float
    median_reported_rate_hz: float
    mean_reported_rate_hz: float
    min_reported_rate_hz: float
    max_reported_rate_hz: float
    counter_rate_hz: float | None
    relative_error: float
    passed: bool
    sample_count: int
    settle_seconds: float
    measurement_seconds: float


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-index", type=int, default=0)
    parser.add_argument("--ip", default="10.0.1.102")
    parser.add_argument("--port", type=int, default=1234)
    parser.add_argument("--min-rate-hz", type=int, default=1000)
    parser.add_argument("--max-rate-hz", type=int, default=15000)
    parser.add_argument(
        "--resolution-hz",
        type=int,
        default=100,
        help="Stop when the passing/failing bracket is this wide (default: 100).",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.10,
        help="Allowed fractional event-rate error (default: 0.10).",
    )
    parser.add_argument(
        "--events-per-trigger",
        type=float,
        default=1.0,
        help="Expected MIDAS events per generator pulse (default: 1).",
    )
    parser.add_argument("--settle-seconds", type=float, default=15.0)
    parser.add_argument("--measurement-seconds", type=float, default=20.0)
    parser.add_argument("--sample-interval", type=float, default=4.0)
    parser.add_argument("--socket-timeout", type=float, default=5.0)
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Output directory; default is a timestamped ignored data directory.",
    )
    parser.add_argument(
        "--leave-at-best",
        action="store_true",
        help="Leave the generator at the highest passing rate instead of restoring it.",
    )
    args = parser.parse_args()
    validate_args(parser, args)
    return args


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if not 0 <= args.frontend_index <= 99:
        parser.error("frontend index must be in [0, 99]")
    if args.min_rate_hz < 1 or args.max_rate_hz <= args.min_rate_hz:
        parser.error("require 1 <= min-rate-hz < max-rate-hz")
    if args.resolution_hz < 1:
        parser.error("resolution-hz must be positive")
    if not 0 < args.tolerance < 1:
        parser.error("tolerance must be between 0 and 1")
    if args.events_per_trigger <= 0:
        parser.error("events-per-trigger must be positive")
    if args.settle_seconds < 0 or args.measurement_seconds <= 0:
        parser.error("settle time must be nonnegative and measurement time positive")
    if args.sample_interval <= 0 or args.sample_interval > args.measurement_seconds:
        parser.error("sample interval must be positive and no longer than measurement time")


def query_float(generator: headless_cli.LecroySocket, command: str) -> float:
    response = generator.query(command)
    try:
        return float(response)
    except ValueError as error:
        raise RuntimeError(f"unexpected {command} response: {response!r}") from error


def set_generator_rate(generator: headless_cli.LecroySocket, rate_hz: int) -> float:
    generator.write("FREQ", str(rate_hz))
    readback = query_float(generator, "FREQ")
    if not math.isclose(readback, rate_hz, rel_tol=1e-4, abs_tol=0.1):
        raise RuntimeError(
            f"generator accepted {readback:g} Hz after requesting {rate_hz} Hz"
        )
    return readback


def read_run_state(client: Any) -> int:
    return int(client.odb_get("/Runinfo/State"))


def read_statistics(client: Any, statistics_path: str, started: float) -> RateSample:
    statistics_node = client.odb_get(statistics_path)
    return RateSample(
        elapsed_s=time.monotonic() - started,
        timestamp_utc=utc_now(),
        events_sent=int(statistics_node["Events sent"]),
        reported_rate_hz=float(statistics_node["Events per sec."]),
        reported_kbytes_per_s=float(statistics_node["kBytes per sec."]),
    )


def measure_trial(
    client: Any,
    generator: headless_cli.LecroySocket,
    statistics_path: str,
    requested_rate_hz: int,
    sequence: int,
    args: argparse.Namespace,
) -> tuple[TrialResult, list[RateSample]]:
    readback = set_generator_rate(generator, requested_rate_hz)
    print(
        f"[{sequence:02d}] requested {requested_rate_hz:5d} Hz; "
        f"readback {readback:9.3f} Hz; settling {args.settle_seconds:g} s",
        flush=True,
    )
    time.sleep(args.settle_seconds)
    if read_run_state(client) != 3:
        raise RuntimeError("MIDAS run stopped during the search")

    samples: list[RateSample] = []
    started = time.monotonic()
    deadline = started + args.measurement_seconds
    while True:
        samples.append(read_statistics(client, statistics_path, started))
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        time.sleep(min(args.sample_interval, remaining))

    rates = [sample.reported_rate_hz for sample in samples]
    median_rate = statistics.median(rates)
    expected_rate = requested_rate_hz * args.events_per_trigger
    lower_limit = expected_rate * (1.0 - args.tolerance)
    upper_limit = expected_rate * (1.0 + args.tolerance)
    counter_rate = None
    if len(samples) >= 2 and samples[-1].elapsed_s > samples[0].elapsed_s:
        counter_delta = samples[-1].events_sent - samples[0].events_sent
        if counter_delta >= 0:
            counter_rate = counter_delta / (
                samples[-1].elapsed_s - samples[0].elapsed_s
            )

    result = TrialResult(
        sequence=sequence,
        requested_rate_hz=requested_rate_hz,
        generator_readback_hz=readback,
        expected_event_rate_hz=expected_rate,
        lower_limit_hz=lower_limit,
        upper_limit_hz=upper_limit,
        median_reported_rate_hz=median_rate,
        mean_reported_rate_hz=statistics.fmean(rates),
        min_reported_rate_hz=min(rates),
        max_reported_rate_hz=max(rates),
        counter_rate_hz=counter_rate,
        relative_error=(median_rate - expected_rate) / expected_rate,
        passed=lower_limit <= median_rate <= upper_limit,
        sample_count=len(samples),
        settle_seconds=args.settle_seconds,
        measurement_seconds=samples[-1].elapsed_s,
    )
    verdict = "PASS" if result.passed else "FAIL"
    counter_text = "n/a" if counter_rate is None else f"{counter_rate:.1f}"
    print(
        f"     {verdict}: MIDAS median={median_rate:.1f} Hz, "
        f"counter={counter_text} Hz, allowed=[{lower_limit:.1f}, {upper_limit:.1f}]",
        flush=True,
    )
    return result, samples


def write_results(
    output_dir: Path,
    args: argparse.Namespace,
    metadata: dict[str, Any],
    trials: list[TrialResult],
    samples_by_rate: dict[int, list[RateSample]],
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    trial_fields = list(TrialResult.__dataclass_fields__)
    with (output_dir / "trials.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=trial_fields)
        writer.writeheader()
        writer.writerows(asdict(trial) for trial in trials)

    with (output_dir / "samples.jsonl").open("w", encoding="utf-8") as output:
        for trial in trials:
            for sample in samples_by_rate[trial.requested_rate_hz]:
                output.write(
                    json.dumps(
                        {
                            "sequence": trial.sequence,
                            "requested_rate_hz": trial.requested_rate_hz,
                            **asdict(sample),
                        },
                        sort_keys=True,
                    )
                    + "\n"
                )

    summary = {
        **metadata,
        "arguments": {
            key: str(value) if isinstance(value, Path) else value
            for key, value in vars(args).items()
        },
        "trials": [asdict(trial) for trial in trials],
    }
    (output_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_dir = args.output_dir or (
        REPO_ROOT
        / "scripts/tools/sampic_tests/data/agilent_midas_rate_search"
        / stamp
    )
    statistics_path = (
        f"/Equipment/SAMPIC {args.frontend_index:02d}/Statistics"
    )

    try:
        client = create_midas_client("agilent_midas_rate_search")
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    generator = None
    initial_rate = None
    trials: list[TrialResult] = []
    samples_by_rate: dict[int, list[RateSample]] = {}
    metadata: dict[str, Any] = {
        "started_utc": utc_now(),
        "frontend_index": args.frontend_index,
        "statistics_path": statistics_path,
        "generator": {"ip": args.ip, "port": args.port},
    }

    try:
        if read_run_state(client) != 3:
            raise RuntimeError("MIDAS must have a run in progress (Runinfo/State != 3)")
        client.odb_get(statistics_path)
        generator = headless_cli.LecroySocket(
            args.ip, args.port, args.socket_timeout, 0.05
        )
        metadata["generator_id"] = generator.query("*IDN")
        try:
            metadata["generator_output"] = generator.query("OUTP")
        except Exception as error:  # Hardware firmware varies; keep scanning.
            metadata["generator_output_error"] = str(error)
        initial_rate = query_float(generator, "FREQ")
        metadata["initial_generator_rate_hz"] = initial_rate

        sequence = 0

        def test(rate_hz: int) -> TrialResult:
            nonlocal sequence
            sequence += 1
            result, samples = measure_trial(
                client,
                generator,
                statistics_path,
                rate_hz,
                sequence,
                args,
            )
            trials.append(result)
            samples_by_rate[rate_hz] = samples
            write_results(output_dir, args, metadata, trials, samples_by_rate)
            return result

        low_result = test(args.min_rate_hz)
        if not low_result.passed:
            best_rate = None
            first_failing_rate = args.min_rate_hz
            conclusion = "minimum_rate_failed"
        else:
            high_result = test(args.max_rate_hz)
            if high_result.passed:
                best_rate = args.max_rate_hz
                first_failing_rate = None
                conclusion = "maximum_rate_passed"
            else:
                low = args.min_rate_hz
                high = args.max_rate_hz
                while high - low > args.resolution_hz:
                    midpoint = (low + high) // 2
                    result = test(midpoint)
                    if result.passed:
                        low = midpoint
                    else:
                        high = midpoint
                best_rate = low
                first_failing_rate = high
                conclusion = "bounded"

        metadata.update(
            {
                "completed_utc": utc_now(),
                "conclusion": conclusion,
                "highest_passing_rate_hz": best_rate,
                "lowest_failing_rate_hz": first_failing_rate,
                "resolution_hz": args.resolution_hz,
            }
        )
        write_results(output_dir, args, metadata, trials, samples_by_rate)
        print(f"Result: highest passing rate = {best_rate} Hz")
        if first_failing_rate is not None:
            print(f"        lowest failing rate = {first_failing_rate} Hz")
        print(f"Data:   {output_dir}")

        if args.leave_at_best and best_rate is not None:
            set_generator_rate(generator, best_rate)
            initial_rate = None
        return 0 if best_rate is not None else 1
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        metadata.update({"completed_utc": utc_now(), "error": str(error)})
        if trials:
            write_results(output_dir, args, metadata, trials, samples_by_rate)
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    finally:
        if generator is not None:
            if initial_rate is not None:
                try:
                    generator.write("FREQ", f"{initial_rate:.12g}")
                    print(f"Restored generator frequency to {initial_rate:g} Hz")
                except Exception as error:  # pylint: disable=broad-except
                    print(f"WARNING: could not restore generator: {error}", file=sys.stderr)
            generator.close()
        client.disconnect()


if __name__ == "__main__":
    raise SystemExit(main())
