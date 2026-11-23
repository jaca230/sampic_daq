#!/usr/bin/env python3
"""Record two simulated crates through the builder using the MIDAS disk logger."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import time

BUILDER = Path(__file__).resolve().parents[2]
PROJECT = BUILDER.parents[1]
sys.path.insert(0, str(BUILDER / 'tests/benchmarks'))
import run_midas_benchmark as harness


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--duration', type=float, default=10)
    parser.add_argument('--rate-hz', type=float, default=100)
    parser.add_argument('--midas-root', type=Path, default=Path(os.environ.get('MIDASSYS', '/home/jack/software/midas')))
    args = parser.parse_args()
    if args.duration <= 0 or args.rate_hz <= 0:
        parser.error('duration and rate must be positive')
    root = PROJECT / '.artifacts/event-builder-qa'
    root.mkdir(parents=True, exist_ok=True)
    artifact = Path(tempfile.mkdtemp(prefix='recording-', dir=root)).resolve()
    data = artifact / 'data'; data.mkdir()
    logs = artifact / 'logs'; logs.mkdir()
    harness.EXPERIMENT = f'SAMPIC_QA_{os.getpid()}'
    environment = harness.initialize_experiment(args.midas_root, artifact)
    config = argparse.Namespace(crates=2, rate_hz=args.rate_hz, hits_per_event=16,
                                samples=64, window_ns=200, select=False)
    harness.configure(environment, PROJECT, config)
    processes = []
    started = False
    try:
        logger = harness.Process('logger', [str(args.midas_root/'bin/mlogger'), '-e', harness.EXPERIMENT], environment, logs)
        processes.append(logger)
        deadline = time.monotonic()+20
        while True:
            logger.require_running()
            ready = harness.run(environment, harness.SYSTEM_PYTHON, '-c', "import midas.client; c=midas.client.MidasClient('qa_logger_ready'); c.odb_get('/Logger/Channels/0/Settings'); c.disconnect()", check=False)
            if ready.returncode == 0: break
            if time.monotonic() >= deadline: raise RuntimeError('logger initialization timeout')
            time.sleep(.2)
        settings = {'/Logger/Data dir': str(data), '/Logger/Write data': True,
                    '/Logger/Channels/0/Settings/Active': True,
                    '/Logger/Channels/0/Settings/Buffer': 'SYSTEM',
                    '/Logger/Channels/0/Settings/Event ID': 2,
                    '/Logger/Channels/0/Settings/Compression': 0,
                    '/Logger/Channels/0/Settings/Compress': 'none',
                    '/Logger/Channels/0/Settings/Filename': 'run%05d.mid',
                    '/Logger/Channels/0/Settings/ODB dump': True}
        harness.run(environment, harness.SYSTEM_PYTHON, '-c', f"import midas.client; c=midas.client.MidasClient('qa_logger_config'); values={settings!r}; [c.odb_set(k,v) for k,v in values.items()]; c.disconnect()")
        for crate in range(2):
            process = harness.Process(f'crate-{crate:02d}', [str(PROJECT/'build/bin/sampic_frontend'), '-i', str(crate)], environment, logs)
            processes.append(process)
            harness.wait_for_equipment(environment, f'SAMPIC {crate:02d}', process)
        builder = harness.Process('builder', [str(BUILDER/'build/sampic_event_builder_frontend')], environment, logs)
        processes.append(builder)
        harness.wait_for_equipment(environment, 'SAMPIC Builder', builder)
        harness.run(environment, str(args.midas_root/'bin/mtransition'), '-e', harness.EXPERIMENT, 'START')
        started = True
        end = time.monotonic()+args.duration
        while time.monotonic() < end:
            for process in processes: process.require_running()
            time.sleep(min(.25, max(0, end-time.monotonic())))
        harness.run(environment, str(args.midas_root/'bin/mtransition'), '-e', harness.EXPERIMENT, 'STOP')
        started = False
        for process in processes: process.require_running()
        variables = harness.read_builder_variables(environment)
        (artifact/'builder-counters.json').write_text(json.dumps(variables, indent=2)+'\n')
        harness.run(environment, harness.SYSTEM_PYTHON, '-c', f"import json,midas.client; c=midas.client.MidasClient('qa_snapshot'); v=c.odb_get('/'); c.disconnect(); open({str(artifact/'odb.json')!r},'w').write(json.dumps(v,indent=2))")
    finally:
        if started:
            harness.run(environment, str(args.midas_root/'bin/mtransition'), '-e', harness.EXPERIMENT, 'STOP', check=False)
        for process in reversed(processes): process.stop()
        harness.run(environment, str(args.midas_root/'bin/odbinit'), '-e', harness.EXPERIMENT, '--cleanup', check=False)
    files = sorted(data.glob('*.mid'))
    if len(files) != 1: raise RuntimeError(f'Expected one MIDAS file in {data}, found {files}')
    recording = files[0]
    validation = harness.run(environment, harness.SYSTEM_PYTHON, '-c', r'''
import collections,json,struct,sys
import midas.file_reader
counts=collections.Counter(); hits=collections.Counter(); reads=collections.Counter()
for event in midas.file_reader.MidasFile(sys.argv[1]):
    if event.header.event_id != 2: continue
    for name,bank in event.banks.items():
        counts[name]+=1; payload=bytes(bank.data)
        if name=='BD00':
            magic,version,reserved,n,start,end=struct.unpack_from('<IHHIQQ',payload)
            assert magic==0x31424553 and version==3 and start<=end
            offset=28
            for _ in range(n):
                crate,board,channel,timestamp,samples=struct.unpack_from('<HHHQI',payload,offset)
                assert samples==64 and crate in (0,1)
                hits[str(crate)]+=1; offset+=18+samples*2
            assert offset==len(payload)
        elif name.startswith('BT'):
            magic,version,size,*v=struct.unpack('<4sHH15Q',payload)
            assert magic==b'BTS1' and version==1 and size==128
            assert v[14]-v[13]+1==v[1]
            reads[name]+=v[1]
        elif name=='BC00':
            magic,version,size,*v=struct.unpack('<4sHH15Q',payload)
            assert magic==b'BCT1' and version==1 and size==128
        else: raise AssertionError('Unexpected bank '+name)
assert counts['BD00'] and counts['BC00'] and reads['BT00'] and reads['BT01']
print(json.dumps(dict(bank_counts=counts,waveforms_by_crate=hits,acquisition_reads_by_bank=reads)))
''', str(recording))
    checked = json.loads(validation.stdout.strip().splitlines()[-1])
    total_hits = sum(checked['waveforms_by_crate'].values())
    total_reads = sum(checked['acquisition_reads_by_bank'].values())
    assert total_hits == harness.numeric(variables, 'input_hits')
    assert total_reads == harness.numeric(variables, 'input_banks')
    assert checked['bank_counts']['BD00'] == harness.numeric(variables, 'output_products')
    assert checked['bank_counts']['BC00'] == harness.numeric(variables, 'diagnostic_events')
    manifest = dict(configuration=vars(config), duration_seconds=args.duration,
                    file=str(recording), bytes=recording.stat().st_size,
                    sha256=hashlib.sha256(recording.read_bytes()).hexdigest(), validation=checked,
                    waveform_format='BD00 v3; int16 ADC samples; no fitting/compression; cuts disabled',
                    timing_formats='BTnn BTS1 v1, BC00 BCT1 v1',
                    signal='Existing simulator pulses, Gaussian noise and jitter; random seeds 1 and 2')
    (artifact/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
