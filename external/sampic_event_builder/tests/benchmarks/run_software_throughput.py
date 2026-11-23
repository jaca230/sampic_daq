#!/usr/bin/env python3
"""Benchmark the software-only MIDAS chain, preserving logs and integrity checks."""
import argparse
import json
import os
import platform
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--crates', default='2')
    parser.add_argument('--rates', default='2000,8000', help='offered vendor events/s/crate')
    parser.add_argument('--hits-per-event', type=int, default=256)
    parser.add_argument('--samples', type=int, default=64)
    parser.add_argument('--duration', type=float, default=10)
    parser.add_argument('--warmup', type=float, default=1)
    parser.add_argument('--select', action='store_true')
    args = parser.parse_args()
    benchmark = Path(__file__).with_name('run_midas_benchmark.py')
    project = benchmark.resolve().parents[4]
    output = project/'.artifacts/software-throughput'
    output.mkdir(parents=True, exist_ok=True)
    current = project/'.artifacts/midas-streaming-benchmark/current'
    (output/'host.json').write_text(json.dumps({'platform':platform.platform(), 'logical_cpus':os.cpu_count(), 'samples':args.samples, 'duration_s':args.duration, 'warmup_s':args.warmup},indent=2)+'\n')
    runs = []
    for crates in map(int,args.crates.split(',')):
        for rate in map(float,args.rates.split(',')):
            name = f'{crates}crates-{rate:g}Hz'+('-cut' if args.select else '')
            destination = output/name
            destination.mkdir(parents=True, exist_ok=True)
            command = [sys.executable,str(benchmark),'--crates',str(crates),'--rate-hz',str(rate),
                       '--hits-per-event',str(args.hits_per_event),'--samples',str(args.samples),
                       '--duration',str(args.duration),'--warmup',str(args.warmup)]
            if args.select: command.append('--select')
            print(f'Running {name}: offered {crates*rate*args.hits_per_event:,.0f} hits/s',flush=True)
            with (destination/'console.log').open('w') as log:
                completed = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
            if (current/'logs').exists(): shutil.copytree(current/'logs',destination/'logs',dirs_exist_ok=True)
            if completed.returncode:
                run = {'name':name,'passed':False,'offered_hits_s':crates*rate*args.hits_per_event}
                print(f'FAILED: see {destination}',flush=True)
            else:
                result=json.loads((current/'results.json').read_text())
                (destination/'results.json').write_text(json.dumps(result,indent=2)+'\n')
                run={'name':name,'passed':result['integrity_passed'],'offered_hits_s':crates*rate*args.hits_per_event,
                     'input_hits_s':result['steady_state_input_hits_per_second'],
                     'finalized_hits_s':result['steady_state_finalized_hits_per_second'],
                     'output_MB_s':result['steady_state_output_megabytes_per_second'],
                     'cpu_cores_used':result['cpu_cores_used'],
                     'cut_enabled':args.select}
                print(f"{run['input_hits_s']:,.0f} input hits/s; {run['finalized_hits_s']:,.0f} finalized hits/s",flush=True)
            runs.append(run)
            (output/'summary.json').write_text(json.dumps(runs,indent=2)+'\n')
            lines=['# Software-only MIDAS throughput','',
                   f'{args.samples}-sample waveforms; {args.duration:g} s measured after {args.warmup:g} s warmup. CPU-only simulation → crate serialization → MIDAS SBUFnn → builder → MIDAS SYSTEM → mdump sink. No disk logger. Warmup and STOP drain excluded from sustained rates. Workers use OS scheduling with no explicit CPU affinity.','',
                   '| Case | Offered hits/s | Input hits/s | Finalized hits/s | Output MB/s | Integrity |',
                   '|---|---:|---:|---:|---:|---|']
            for r in runs:
                lines.append(f"| {r['name']} | {r['offered_hits_s']:,.0f} | {r.get('input_hits_s',0):,.0f} | {r.get('finalized_hits_s',0):,.0f} | {r.get('output_MB_s',0):.1f} | {'PASS' if r['passed'] else 'FAIL'} |")
            lines.extend(['','Integrity: frontends survived (overflow is fatal), zero malformed input banks, and input/finalized hit equality with cuts disabled. Does not verify every output sample or measure disk logging.'])
            (output/'report.md').write_text('\n'.join(lines)+'\n')
    print(output/'report.md')
    return 1 if any(not run['passed'] for run in runs) else 0

if __name__=='__main__': raise SystemExit(main())
