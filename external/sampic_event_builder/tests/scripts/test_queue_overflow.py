#!/usr/bin/env python3
"""Force each frontend stage queue to overflow in isolated simulator experiments."""
import argparse
import os
from pathlib import Path
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT.parents[1]
sys.path.insert(0, str(ROOT / 'tests/benchmarks'))
import run_midas_benchmark as harness


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--midas-root', type=Path, default=Path(os.environ.get('MIDASSYS', '/home/jack/software/midas')))
    args = parser.parse_args()
    for stage in ('input', 'output'):
        harness.EXPERIMENT = f'SAMPIC_Q_{os.getpid()}_{stage}'
        artifact_root = PROJECT / '.artifacts/queue-overflow-tests'
        artifact_root.mkdir(parents=True, exist_ok=True)
        artifact = Path(tempfile.mkdtemp(prefix=stage+'-', dir=artifact_root))
        environment = harness.initialize_experiment(args.midas_root, artifact)
        config = argparse.Namespace(crates=1, rate_hz=1000, hits_per_event=1,
                                    samples=64, window_ns=200, select=False)
        harness.configure(environment, PROJECT, config)
        setting = '/Equipment/SAMPIC 00/Settings'
        overrides = {
            setting+'/Sampic Event Collector/buffer_size': 10 if stage=='input' else 4096,
            setting+'/Frontend Event Collector/buffer_size': 128 if stage=='input' else 1,
            setting+'/Frontend Event Collector/sleep_time_us': 500000 if stage=='input' else 100000,
            setting+'/Sampic Event Collector/modes/simulator/events_per_cycle': 9 if stage=='input' else 32,
            setting+'/Sampic Event Collector/modes/simulator/simulate_read_time_us': 100000 if stage=='input' else 10000,
        }
        harness.run(environment, harness.SYSTEM_PYTHON, '-c', f'''
import midas.client
c=midas.client.MidasClient('configure_queue_test')
for path,value in {overrides!r}.items(): c.odb_set(path,value)
c.disconnect()
''')
        logs = artifact / 'logs'; logs.mkdir()
        processes = []
        try:
            producer = harness.Process('producer', [str(PROJECT/'build/bin/sampic_frontend'), '-i', '0'], environment, logs)
            processes.append(producer)
            harness.wait_for_equipment(environment, 'SAMPIC 00', producer)
            processes.append(harness.Process('sink', [str(args.midas_root/'bin/mdump'), '-s', '-i', '1', '-g', 'all', '-z', 'SBUF00', '-e', harness.EXPERIMENT], environment, logs))
            harness.run(environment, str(args.midas_root/'bin/mtransition'), '-e', harness.EXPERIMENT, 'START')
            deadline = time.monotonic()+15
            while producer.process.poll() is None and time.monotonic()<deadline:
                time.sleep(0.1)
            if producer.process.poll() is None: raise RuntimeError(f'{stage}: overflow did not stop frontend; logs {logs}')
            producer.log.flush()
            text = producer.log_path.read_text()
            expected = 'SAMPIC input' if stage=='input' else 'Frontend output'
            if f'{expected} queue overflow (capacity=' not in text or 'data incomplete' not in text or 'Shutting down frontend runtime' not in text:
                raise RuntimeError(f'{stage}: missing fatal reason or cleanup; logs {logs}')
            if 'SAMPIC Collector stopped' not in text:
                raise RuntimeError(f'{stage}: acquisition collector did not stop; logs {logs}')
            print(f'{stage}: immediate overflow failure and frontend cleanup verified; logs {logs}', flush=True)
        finally:
            harness.run(environment, str(args.midas_root/'bin/mtransition'), '-e', harness.EXPERIMENT, 'STOP', check=False)
            for process in reversed(processes): process.stop()
            harness.run(environment, str(args.midas_root/'bin/odbinit'), '-e', harness.EXPERIMENT, '--cleanup', check=False)
    return 0

if __name__=='__main__': raise SystemExit(main())
