#!/usr/bin/env python3
"""Run simulated crates, the builder, and a local MIDAS webpage until Ctrl+C."""
import argparse
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

BUILDER_ROOT = Path(__file__).resolve().parents[2]
PROJECT_ROOT = BUILDER_ROOT.parents[1]
sys.path.insert(0, str(BUILDER_ROOT / "tests/benchmarks"))
import run_midas_benchmark as harness


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crates", type=int, default=2)
    parser.add_argument("--rate-hz", type=float, default=1000)
    parser.add_argument("--hits-per-event", type=int, default=16)
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--window-ns", type=int, default=200)
    parser.add_argument("--select", action="store_true", help="Enable punch-through rejection")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--midas-root", type=Path, default=Path(os.environ.get("MIDASSYS", "/home/jack/software/midas")))
    parser.add_argument("--producer", type=Path, default=PROJECT_ROOT / "build/bin/sampic_frontend")
    parser.add_argument("--builder", type=Path, default=Path(os.environ.get("SAMPIC_EVENT_BUILDER_BUILD_DIR", str(BUILDER_ROOT / "build"))) / "sampic_event_builder_frontend")
    parser.add_argument('--collection-strategy',choices=['periodic_readout','time_grouping','fixed_time_bins'],default='periodic_readout')
    parser.add_argument('--period-ns',type=int,default=10_000_000)
    args = parser.parse_args()
    if not 1 <= args.crates <= 100 or args.rate_hz <= 0 or not 1 <= args.samples <= 64 or args.hits_per_event <= 0 or args.window_ns <= 0:
        parser.error("crates must be 1-100, samples 1-64, and rate/hits/window positive")
    for binary in (args.producer, args.builder, args.midas_root / "bin/mhttpd"):
        if not binary.is_file(): parser.error(f"missing executable: {binary}; see README build instructions")
    # Fail before starting anything if another webpage is using this port.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", args.port))
    artifact_root = PROJECT_ROOT / ".artifacts/midas-simulator"
    artifact_root.mkdir(parents=True, exist_ok=True)
    artifact = Path(tempfile.mkdtemp(prefix="session-", dir=artifact_root))
    harness.EXPERIMENT = f"SAMPIC_SIM_{os.getpid()}"
    environment = harness.initialize_experiment(args.midas_root, artifact)
    harness.configure(environment, PROJECT_ROOT, args)
    # Keep the demo webpage on localhost and avoid other MIDAS web ports.
    harness.run(environment, harness.SYSTEM_PYTHON, "-c", f'''
import midas.client
c = midas.client.MidasClient('configure_sim_web')
for path, value in {{'/WebServer/Enable localhost port': True,
                    '/WebServer/localhost port': {args.port},
                    '/WebServer/Enable insecure port': False,
                    '/WebServer/Enable https port': False}}.items(): c.odb_set(path, value)
c.disconnect()
''')
    logs = artifact / "logs"
    logs.mkdir()
    processes = []
    started = False
    def interrupt(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupt)
    try:
        for crate in range(args.crates):
            process = harness.Process(f"crate-{crate:02d}", [str(args.producer), "-i", str(crate)], environment, logs)
            processes.append(process)
            harness.wait_for_equipment(environment, f"SAMPIC {crate:02d}", process)
        builder = harness.Process("builder", [str(args.builder)], environment, logs)
        processes.append(builder)
        harness.wait_for_equipment(environment, "SAMPIC Builder", builder)
        # Consume built events so SYSTEM does not fill during a long demo.
        for name, command in (
            ("sink", [str(args.midas_root / "bin/mdump"), "-s", "-i", "2", "-g", "all", "-z", "SYSTEM", "-e", harness.EXPERIMENT]),
            ("web", [str(args.midas_root / "bin/mhttpd"), "-e", harness.EXPERIMENT]),
        ):
            processes.append(harness.Process(name, command, environment, logs))
        time.sleep(0.5)
        for process in processes: process.require_running()
        harness.run(environment, str(args.midas_root / "bin/mtransition"), "-e", harness.EXPERIMENT, "START")
        started = True
        print(f"MIDAS simulator running: http://localhost:{args.port}", flush=True)
        print(f"Experiment: {harness.EXPERIMENT}\nLogs: {logs}\nWatch the SAMPIC crate equipment and SAMPIC Builder on Status; Ctrl+C stops the run and processes.", flush=True)
        while True:
            for process in processes: process.require_running()
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        if started:
            harness.run(environment, str(args.midas_root / "bin/mtransition"), "-e", harness.EXPERIMENT, "STOP", check=False)
        for process in reversed(processes): process.stop()
        harness.run(environment, str(args.midas_root / "bin/odbinit"), "-e", harness.EXPERIMENT, "--cleanup", check=False)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
