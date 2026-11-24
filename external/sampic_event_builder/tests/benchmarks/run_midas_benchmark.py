#!/usr/bin/env python3
"""Benchmark simulated SAMPIC crates through MIDAS and the event builder."""

from __future__ import annotations

import argparse
import getpass
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time


EXPERIMENT = "SAMPIC_STREAMING_BENCHMARK"
SYSTEM_PYTHON = "/usr/bin/python3"


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crates", type=int, default=2)
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--rate-hz", type=float, default=1000.0,
                        help="generated vendor events per second per crate")
    parser.add_argument("--hits-per-event", type=int, default=16)
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--warmup", type=float, default=1.0, help="warmup seconds before the measurement interval")
    parser.add_argument("--window-ns", type=int, default=200)
    parser.add_argument("--collection-strategy",choices=["periodic_readout","time_grouping","fixed_time_bins"],default="periodic_readout")
    parser.add_argument("--period-ns",type=int,default=10_000_000)
    parser.add_argument("--select", action="store_true")
    parser.add_argument("--profile-builder", action="store_true",
                        help="Record a perf profile of the MIDAS builder.")
    parser.add_argument("--strace-builder", action="store_true",
                        help="Record a syscall summary of the MIDAS builder.")
    parser.add_argument("--midas-root", type=Path,
                        default=Path(os.environ.get("MIDASSYS", "/home/jack/software/midas")))
    return parser.parse_args()


class Process:
    def __init__(self, name: str, command: list[str], environment: dict[str, str], logs: Path):
        self.name = name
        self.log_path = logs / f"{name}.log"
        self.log = self.log_path.open("w", encoding="utf-8")
        self.process = subprocess.Popen(
            command, env=environment, stdout=self.log, stderr=subprocess.STDOUT,
            start_new_session=True)

    def require_running(self) -> None:
        status = self.process.poll()
        if status is not None:
            raise RuntimeError(
                f"{self.name} exited with status {status}; see {self.log_path}")

    def stop(self) -> None:
        if self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGTERM)
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait(timeout=3)
        self.log.close()


def run(environment: dict[str, str], *command: str, check: bool = True):
    result = subprocess.run(command, env=environment, check=False, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    if check and result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command!r}\n{result.stdout}")
    return result


def initialize_experiment(midas_root: Path, artifact: Path) -> dict[str, str]:
    experiment_dir = artifact / "experiment"
    experiment_dir.mkdir(parents=True)
    exptab = experiment_dir / "exptab"
    exptab.write_text(
        f"{EXPERIMENT} {experiment_dir} {getpass.getuser()}\n", encoding="utf-8")
    environment = os.environ.copy()
    environment.update({
        "MIDASSYS": str(midas_root.resolve()),
        "MIDAS_EXPTAB": str(exptab.resolve()),
        "MIDAS_EXPT_NAME": EXPERIMENT,
        "PATH": f"{midas_root.resolve() / 'bin'}:{environment.get('PATH', '')}",
        "PYTHONPATH": f"{midas_root.resolve() / 'python'}:{environment.get('PYTHONPATH', '')}",
        # Avoid unrelated Conda/ROOT OpenSSL libraries shadowing the system
        # libraries required by MIDAS's Python bindings.
        "LD_LIBRARY_PATH": "",
    })

    # MIDAS may have been built with a Conda OpenSSL RPATH. Python imports
    # system OpenSSL before loading MIDAS; preload MIDAS's linked pair so both
    # use the same ABI. This affects only this isolated experiment's children.
    compat_library = midas_root.resolve() / "lib/libmidas-c-compat.so"
    if sys.platform.startswith("linux") and compat_library.is_file():
        linked = subprocess.run(["ldd", str(compat_library)], capture_output=True, text=True, check=True)
        openssl = []
        for line in linked.stdout.splitlines():
            fields = line.split()
            if len(fields) >= 3 and fields[0].startswith(("libssl.so", "libcrypto.so")) and Path(fields[2]).is_file():
                openssl.append(fields[2])
        if len(openssl) == 2:
            environment["LD_PRELOAD"] = ":".join(openssl + ([environment["LD_PRELOAD"]] if environment.get("LD_PRELOAD") else []))

    prefix = f"{os.getuid()}_{EXPERIMENT}_"
    shm = Path("/dev/shm")
    if shm.is_dir():
        for path in shm.iterdir():
            if path.name.startswith(prefix):
                path.unlink()
    for attempt in range(3):
        run(environment, str(midas_root / "bin/odbinit"), "-e", EXPERIMENT,
            "--cleanup", check=False)
        for stale in experiment_dir.glob(".*.SHM"):
            stale.unlink(missing_ok=True)
        for name in (".ODB_SIZE.TXT", ".SHM_HOST.TXT", ".SHM_TYPE.TXT"):
            (experiment_dir / name).unlink(missing_ok=True)
        created = run(
            environment, str(midas_root / "bin/odbinit"), "-e", EXPERIMENT,
            "-s", "96MB", check=False)
        snapshot = experiment_dir / ".ODB.SHM"
        magic = snapshot.read_bytes()[:4] if snapshot.is_file() else b""
        if magic == b"ODB\0":
            # On WSL, preserve the valid snapshot and let the first client
            # reopen it through MIDAS's normal POSIX-shared-memory path.
            run(environment, str(midas_root / "bin/odbinit"), "-e", EXPERIMENT,
                "--cleanup", check=False)
            time.sleep(0.25)
            return environment
        if attempt == 2:
            raise RuntimeError(
                "could not initialize MIDAS ODB:\n" + created.stdout)
        time.sleep(0.2 * (attempt + 1))
    raise RuntimeError("could not initialize MIDAS ODB")


def simulator_pacing(rate_hz: float) -> tuple[int, int]:
    # A sleep for every generated event becomes the source bottleneck above a
    # few kHz: sub-100-us sleeps routinely overshoot by tens of microseconds.
    # Generate a batch per approximately 1-ms pacing interval instead. This
    # preserves the requested average event rate without measuring scheduler
    # wake-up latency as if it were event-builder capacity.
    events_per_cycle = max(1, round(rate_hz / 1000.0))
    read_time_us = max(
        1, round(1_000_000.0 * events_per_cycle / rate_hz))
    return events_per_cycle, read_time_us


def configure(environment: dict[str, str], project: Path, args: argparse.Namespace) -> None:
    profile = project / "scripts/odb_tools/profiles/apply_profile.py"
    events_per_cycle, read_time_us = simulator_pacing(args.rate_hz)
    for crate in range(args.crates):
        run(environment, SYSTEM_PYTHON, str(profile), "streaming_simulator",
            "--frontend-index", str(crate),
            "--events-per-cycle", str(events_per_cycle),
            "--hits-per-event", str(args.hits_per_event),
            "--waveform-length", str(args.samples),
            "--read-time-us", str(read_time_us),
            "--inter-event-gap-ns", str(max(args.window_ns * 2, 1000)),
            "--apply")

    script = """
import midas.client
c = midas.client.MidasClient('configure_sampic_builder')
values = %s
for path, value in values.items():
    if path.endswith('/periodic_readout/Period ns'):
        c.odb_set(path,value,explicit_new_midas_type=midas.TID_QWORD)
    else: c.odb_set(path,value)
common = '/Equipment/SAMPIC Builder/Common'
c.odb_set(common+'/Event ID', 2, explicit_new_midas_type=midas.TID_WORD)
for key,value in {'Trigger mask': 0, 'Buffer': 'SYSTEM', 'Type': 256,
                  'Source': 0, 'Format': 'MIDAS', 'Enabled': True,
                  'Read on': 1, 'Period': 10, 'Event limit': 0,
                  'Num subevents': 0, 'Log history': 0}.items():
    c.odb_set(common+'/'+key,value)
c.disconnect()
""" % repr({
        "/Equipment/SAMPIC Builder/Settings/Collection_strategies/time_grouping/Event window ns": args.window_ns,
        "/Equipment/SAMPIC Builder/Settings/Collection_strategies/time_grouping/Allowed lateness ns":
            max(2000, args.window_ns * 4),
        "/Equipment/SAMPIC Builder/Settings/Collection_strategies/Active strategy": getattr(args,"collection_strategy","periodic_readout"),
        "/Equipment/SAMPIC Builder/Settings/Collection_strategies/periodic_readout/Period ns": getattr(args,"period_ns",10_000_000),
        "/Equipment/SAMPIC Builder/Settings/Expected crates": args.crates,
        "/Equipment/SAMPIC Builder/Settings/Input buffers": ','.join(f'SBUF{i:02d}' for i in range(args.crates)),
        "/Equipment/SAMPIC Builder/Settings/Selection/topology/Enabled": args.select,
        "/Equipment/SAMPIC Builder/Settings/Selection/topology/Plane count": 8,
        "/Equipment/SAMPIC Builder/Settings/Selection/topology/Channels per plane": 32,
        "/Equipment/SAMPIC Builder/Settings/Selection/topology/Maximum punch integral per hit": 3000,
    })
    run(environment, SYSTEM_PYTHON, "-c", script)


def read_statistics(environment: dict[str, str], equipment: list[str]) -> dict[str, dict]:
    script = """
import json, midas.client, os
c = midas.client.MidasClient('read_sampic_stats_' + str(os.getpid()))
names = %s
result = {}
for name in names:
    path = '/Equipment/' + name + '/Statistics'
    try: result[name] = c.odb_get(path)
    except KeyError: result[name] = {}
c.disconnect()
print(json.dumps(result))
""" % repr(equipment)
    result = run(environment, SYSTEM_PYTHON, "-c", script)
    return json.loads(result.stdout.strip().splitlines()[-1])


def read_builder_variables(environment: dict[str, str]) -> dict:
    script = """
import json, midas.client, os
c = midas.client.MidasClient('read_builder_variables_' + str(os.getpid()))
try: result = c.odb_get('/Equipment/SAMPIC Builder/Variables')
except KeyError: result = {}
c.disconnect()
print(json.dumps(result))
"""
    result = run(environment, SYSTEM_PYTHON, "-c", script)
    return json.loads(result.stdout.strip().splitlines()[-1])


def cpu_seconds(process: Process) -> float:
    fields = Path(f'/proc/{process.process.pid}/stat').read_text().rsplit(')', 1)[1].split()
    return (int(fields[11])+int(fields[12])) / os.sysconf('SC_CLK_TCK')


def wait_for_equipment(
        environment: dict[str, str], name: str, process: Process) -> None:
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        process.require_running()
        try:
            if read_statistics(environment, [name]).get(name):
                return
        except (subprocess.CalledProcessError, RuntimeError):
            # The frontend creates its ODB records during startup. A second
            # short-lived client can transiently fail while that initialization
            # is in progress; retry after the equipment process settles.
            pass
        time.sleep(0.2)
    raise RuntimeError(f"MIDAS equipment did not appear: {name}")


def numeric(stats: dict, *names: str) -> float:
    lowered = {key.lower().replace(" ", "_"): value for key, value in stats.items()}
    for name in names:
        if name in lowered:
            return float(lowered[name])
    return 0.0


def main() -> int:
    args = arguments()
    events_per_cycle, read_time_us = simulator_pacing(args.rate_hz)
    if not 1 <= args.crates <= 100 or args.duration <= 0 or args.rate_hz <= 0:
        raise ValueError("crates must be 1-100 and duration/rate must be positive")
    if not 1 <= args.samples <= 64 or args.hits_per_event <= 0:
        raise ValueError("samples must be 1-64 and hits must be positive")

    event_builder = Path(__file__).resolve().parents[2]
    project = event_builder.parents[1]
    artifact = project / ".artifacts/midas-streaming-benchmark/current"
    if artifact.exists():
        shutil.rmtree(artifact)
    logs = artifact / "logs"
    logs.mkdir(parents=True)

    producer = project / "build/bin/sampic_frontend"
    builder = Path(os.environ.get("SAMPIC_EVENT_BUILDER_BUILD_DIR", str(event_builder / "build"))) / "sampic_event_builder_frontend"
    for binary in (producer, builder):
        if not binary.is_file():
            raise RuntimeError(f"missing benchmark executable: {binary}")

    environment = initialize_experiment(args.midas_root, artifact)
    configure(environment, project, args)
    processes: list[Process] = []
    equipment = [f"SAMPIC {crate:02d}" for crate in range(args.crates)]
    equipment.append("SAMPIC Builder")
    started = time.monotonic()
    try:
        for crate in range(args.crates):
            process = Process(
                f"crate-{crate:02d}", [str(producer), "-i", str(crate)],
                environment, logs)
            processes.append(process)
            wait_for_equipment(environment, f"SAMPIC {crate:02d}", process)
        builder_command = [str(builder)]
        if args.profile_builder:
            builder_command = [
                "perf", "record", "-q", "-g", "-o",
                str(artifact / "event-builder.perf.data"), "--", str(builder),
            ]
        elif args.strace_builder:
            builder_command = [
                "strace", "-f", "-c", "-o",
                str(artifact / "event-builder.strace.txt"), str(builder),
            ]
        process = Process("event-builder", builder_command, environment, logs)
        processes.append(process)
        wait_for_equipment(environment, "SAMPIC Builder", process)

        # Only the event builder publishes into SYSTEM; crates use SBUFnn. A real deployment has
        # a logger or downstream consumer; without one, a saturation benchmark
        # would only measure how quickly that finite buffer fills.
        sink = Process(
            "output-sink",
            [str(args.midas_root / "bin/mdump"), "-s", "-i", "2",
             "-g", "all", "-z", "SYSTEM", "-e", EXPERIMENT],
            environment, logs)
        processes.append(sink)
        time.sleep(0.25)
        sink.require_running()

        run(environment, str(args.midas_root / "bin/mtransition"),
            "-e", EXPERIMENT, "START")
        run_started = time.monotonic()
        time.sleep(args.warmup)
        baseline = read_builder_variables(environment)
        cpu_start = {p.name:cpu_seconds(p) for p in processes}
        benchmark_start = time.monotonic()
        while time.monotonic() - benchmark_start < args.duration:
            time.sleep(min(1.0, args.duration))
            for process in processes:
                process.require_running()
        measurement_elapsed = time.monotonic() - benchmark_start
        acquisition_elapsed = time.monotonic() - run_started
        cpu_used = {p.name:(cpu_seconds(p)-cpu_start[p.name])/measurement_elapsed for p in processes}
        live_builder_variables = read_builder_variables(environment)
        run(environment, str(args.midas_root / "bin/mtransition"),
            "-e", EXPERIMENT, "STOP")
        pipeline_elapsed = time.monotonic() - run_started
        # Standard equipment statistics are published periodically. Read them
        # after STOP, when frontends have committed their final counters.
        time.sleep(0.5)
        after = read_statistics(environment, equipment)
        builder_variables = read_builder_variables(environment)
        for process in processes:
            process.require_running() # Any queue overflow terminates its frontend.
        metrics = builder_variables.get('Stages', {})
        if not numeric(builder_variables, 'input_hits'):
            raise RuntimeError("No input hits received: check source Common settings and buffer routing")
        if numeric(builder_variables, 'malformed_banks'):
            raise RuntimeError('Malformed builder input banks')
        if not args.select and numeric(metrics.get('Finalization',{}), 'hits') != numeric(builder_variables, 'input_hits'):
            raise RuntimeError('Input/finalized hit mismatch after STOP')

        for counter in ('acquisition_timing_records', 'summarized_timing_reads'):
            if numeric(builder_variables, counter) != numeric(builder_variables, 'input_banks'):
                raise RuntimeError(f'Timing accounting mismatch: {counter}')
        if not numeric(builder_variables, 'diagnostic_events'):
            raise RuntimeError('No collector timing events emitted')

        rates = {}
        for name in equipment[:-1]:
            sent = numeric(after.get(name, {}), "events_sent")
            rates[name] = max(0.0, sent) / acquisition_elapsed
        output_products = numeric(builder_variables, "output_products")
        rates["SAMPIC Builder"] = output_products / acquisition_elapsed
        input_banks = numeric(builder_variables, "input_banks")
        input_hits = numeric(builder_variables, "input_hits")
        input_bank_bytes = numeric(builder_variables, "input_bank_bytes")
        output_bytes = numeric(builder_variables, "output_bytes")
        live_elapsed = (numeric(live_builder_variables, "metrics_elapsed_ns") - numeric(baseline, "metrics_elapsed_ns")) / 1.0e9
        live_input_hits = numeric(live_builder_variables, "input_hits") - numeric(baseline, "input_hits")
        live_input_bytes = numeric(live_builder_variables, "input_bank_bytes") - numeric(baseline, "input_bank_bytes")
        live_output_bytes = numeric(live_builder_variables, "output_bytes") - numeric(baseline, "output_bytes")
        selected_events = numeric(builder_variables, "selection_kept_events")
        rejected_events = numeric(builder_variables, "selection_rejected_events")
        stage_results = {}
        for stage_name, stage in builder_variables.get("Stages", {}).items():
            hits = numeric(stage, "hits")
            input_bytes = numeric(stage, "input_bytes")
            stage_output_bytes = numeric(stage, "output_bytes")
            processing_ns = numeric(stage, "processing_ns")
            service_seconds = processing_ns / 1.0e9
            stage_results[stage_name] = {
                "hits": int(hits),
                "input_bytes": int(input_bytes),
                "output_bytes": int(stage_output_bytes),
                "processing_nanoseconds": int(processing_ns),
                "observed_hits_per_second": hits / acquisition_elapsed,
                "observed_input_megabytes_per_second": input_bytes / acquisition_elapsed / 1.0e6,
                "observed_output_megabytes_per_second": stage_output_bytes / acquisition_elapsed / 1.0e6,
                "service_hits_per_second": hits / service_seconds if service_seconds else 0.0,
                "service_input_megabytes_per_second":
                    input_bytes / service_seconds / 1.0e6 if service_seconds else 0.0,
            }
        result = {
            "cpu_cores_used": cpu_used,
            "integrity_passed": True,
            "acquisition_timing_records": int(numeric(builder_variables, 'acquisition_timing_records')),
            "summarized_timing_reads": int(numeric(builder_variables, 'summarized_timing_reads')),
            "diagnostic_events": int(numeric(builder_variables, 'diagnostic_events')),
            "steady_state_finalized_hits_per_second":
                (numeric(live_builder_variables.get('Stages',{}).get('Finalization',{}), 'hits')-
                 numeric(baseline.get('Stages',{}).get('Finalization',{}), 'hits')) / live_elapsed if live_elapsed else 0.0,
            "configuration": vars(args) | {
                "midas_root": str(args.midas_root),
                "simulator_events_per_cycle": events_per_cycle,
                "simulator_read_time_us": read_time_us,
            },
            "acquisition_elapsed_seconds": acquisition_elapsed,
            "measurement_elapsed_seconds": measurement_elapsed,
            "pipeline_elapsed_seconds": pipeline_elapsed,
            "event_rates_hz": rates,
            "input_banks": int(input_banks),
            "input_hits": int(input_hits),
            "input_bank_bytes": int(input_bank_bytes),
            "output_products": int(output_products),
            "output_bytes": int(output_bytes),
            "selected_events": int(selected_events),
            "rejected_events": int(rejected_events),
            "selection_rejection_fraction": rejected_events /
                (selected_events + rejected_events) if selected_events + rejected_events else 0.0,
            "input_hits_per_second": input_hits / acquisition_elapsed,
            "sustained_input_hits_per_second": input_hits / pipeline_elapsed,
            "steady_state_input_hits_per_second":
                live_input_hits / live_elapsed if live_elapsed else 0.0,
            "steady_state_input_bank_megabytes_per_second":
                live_input_bytes / live_elapsed / 1.0e6 if live_elapsed else 0.0,
            "steady_state_output_megabytes_per_second":
                live_output_bytes / live_elapsed / 1.0e6 if live_elapsed else 0.0,
            "sustained_input_bank_megabytes_per_second":
                input_bank_bytes / pipeline_elapsed / 1_000_000.0,
            "output_events_per_second": rates["SAMPIC Builder"],
            "output_megabytes_per_second": output_bytes / acquisition_elapsed / 1_000_000.0,
            "sustained_output_megabytes_per_second":
                output_bytes / pipeline_elapsed / 1_000_000.0,
            "input_output_byte_ratio":
                input_bank_bytes / output_bytes if output_bytes else 0.0,
            "statistics_snapshot": after,
            "builder_variables": builder_variables,
            "live_builder_variables": live_builder_variables,
            "stages": stage_results,
            "wall_seconds": time.monotonic() - started,
        }
        (artifact / "results.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8")
        lines = [
            "# SAMPIC MIDAS streaming benchmark", "",
            f"- Crates: {args.crates}",
            f"- Punch-through cut enabled: {args.select}",
            f"- Raw samples/hit: {args.samples}",
            f"- Hits/vendor event: {args.hits_per_event}",
            f"- Requested rate/crate: {args.rate_hz:.1f} Hz",
            f"- Measured input hit rate: {result['input_hits_per_second']:.1f} hits/s",
            f"- Measured built-event rate: {result['output_events_per_second']:.1f} events/s",
            f"- Product throughput: {result['output_megabytes_per_second']:.3f} MB/s",
            f"- Input banks / hits: {result['input_banks']} / {result['input_hits']}",
            f"- Output products / bytes: {result['output_products']} / {result['output_bytes']}",
            f"- Rejected punch-through candidates: {result['rejected_events']} "
            f"({100.0 * result['selection_rejection_fraction']:.2f}%)",
            "", "| Equipment | Events/s |", "|---|---:|",
        ]
        lines.extend(f"| {name} | {rate:.1f} |" for name, rate in rates.items())
        lines.extend([
            "", "| Stage | Observed hits/s | Observed input MB/s | "
            "Observed output MB/s | Service capacity hits/s | Service capacity MB/s |",
            "|---|---:|---:|---:|---:|---:|",
        ])
        lines.extend(
            f"| {name} | {stage['observed_hits_per_second']:.1f} | "
            f"{stage['observed_input_megabytes_per_second']:.3f} | "
            f"{stage['observed_output_megabytes_per_second']:.3f} | "
            f"{stage['service_hits_per_second']:.1f} | "
            f"{stage['service_input_megabytes_per_second']:.3f} |"
            for name, stage in stage_results.items())
        (artifact / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        print(f"Report: {artifact / 'report.md'}")
        return 0
    finally:
        run(environment, str(args.midas_root / "bin/mtransition"),
            "-e", EXPERIMENT, "STOP", check=False)
        for process in reversed(processes):
            process.stop()


if __name__ == "__main__":
    raise SystemExit(main())
