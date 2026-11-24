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
    parser.add_argument('--rate-hz', type=float, default=500)
    parser.add_argument('--dummy-cut',action='store_true')
    parser.add_argument('--dummy-every',type=int,default=3)
    parser.add_argument('--dummy-priority',type=int,default=200)
    parser.add_argument('--topology-cut',action='store_true')
    parser.add_argument('--topology-priority',type=int,default=100)
    parser.add_argument('--topology-planes',type=int,default=8)
    parser.add_argument('--topology-channels',type=int,default=32)
    parser.add_argument('--topology-max-integral',type=int,default=3000)
    parser.add_argument('--no-timing',action='store_true')
    parser.add_argument('--midas-root', type=Path, default=Path(os.environ.get('MIDASSYS', '/home/jack/software/midas')))
    parser.add_argument('--collection-strategy',choices=['periodic_readout','time_grouping','fixed_time_bins'],default='periodic_readout')
    parser.add_argument('--period-ns',type=int,default=10_000_000)
    args = parser.parse_args()
    if args.duration <= 0 or args.rate_hz <= 0 or args.period_ns <= 0:
        parser.error('duration and rate must be positive')
    root = PROJECT / '.artifacts/event-builder-qa'
    root.mkdir(parents=True, exist_ok=True)
    artifact = Path(tempfile.mkdtemp(prefix='recording-', dir=root)).resolve()
    data = artifact / 'data'; data.mkdir()
    logs = artifact / 'logs'; logs.mkdir()
    harness.EXPERIMENT = f'SAMPIC_QA_{os.getpid()}'
    environment = harness.initialize_experiment(args.midas_root, artifact)
    config = argparse.Namespace(crates=2, rate_hz=args.rate_hz, hits_per_event=16,
                                samples=64, window_ns=200, select=args.topology_cut,
                                collection_strategy=args.collection_strategy,period_ns=args.period_ns,
                                dummy_enabled=args.dummy_cut,dummy_every=args.dummy_every,
                                dummy_priority=args.dummy_priority,topology_priority=args.topology_priority,
                                timing_enabled=not args.no_timing,topology_max_integral=args.topology_max_integral,
                                topology_planes=args.topology_planes,topology_channels=args.topology_channels)
    harness.configure(environment, PROJECT, config)
    settings={
        '/Equipment/SAMPIC Builder/Settings/Selection/dummy/Enabled':args.dummy_cut,
        '/Equipment/SAMPIC Builder/Settings/Selection/dummy/Priority':args.dummy_priority,
        '/Equipment/SAMPIC Builder/Settings/Selection/dummy/Reject every N events':args.dummy_every,
        '/Equipment/SAMPIC Builder/Settings/Selection/topology/Priority':args.topology_priority,
        '/Equipment/SAMPIC Builder/Settings/Selection/topology/Plane count':args.topology_planes,
        '/Equipment/SAMPIC Builder/Settings/Selection/topology/Channels per plane':args.topology_channels,
        '/Equipment/SAMPIC Builder/Settings/Selection/topology/Maximum punch integral per hit':args.topology_max_integral,
        '/Equipment/SAMPIC Builder/Settings/Timing/Enabled':not args.no_timing,
    }
    harness.run(environment,harness.SYSTEM_PYTHON,'-c',f"import midas.client; c=midas.client.MidasClient('qa_cuts_config'); values={settings!r}; [c.odb_set(k,v) for k,v in values.items()]; c.disconnect()")
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
    for log in logs.glob('*.log'):
        text=log.read_text(errors='replace')
        if 'Cannot read ODB setting' in text:
            raise RuntimeError(f'Invalid ODB configuration in {log}')
    files = sorted(data.glob('*.mid'))
    if len(files) != 1: raise RuntimeError(f'Expected one MIDAS file in {data}, found {files}')
    recording = files[0]
    validation = harness.run(environment, harness.SYSTEM_PYTHON, '-c', r'''
import collections,json,struct,sys
import midas.file_reader
last_cycle=0; last_serial=-1
counts=collections.Counter(); hits=collections.Counter(); reads=collections.Counter(); cuts=collections.defaultdict(collections.Counter)
for event in midas.file_reader.MidasFile(sys.argv[1]):
    if event.header.event_id != 2: continue
    assert event.header.serial_number>last_serial
    last_serial=event.header.serial_number
    for name,bank in event.banks.items():
        counts[name]+=1; payload=bytes(bank.data)
        if name=='BD00':
            magic,version,reserved,n,start,end=struct.unpack_from('<IHHIQQ',payload)
            assert magic==0x31424553 and version==4 and start<=end
            offset=28
            for _ in range(n):
                crate,board,channel=struct.unpack_from('<HBB',payload,offset)
                samples=payload[offset+2+8]
                timestamp=struct.unpack_from('<d',payload,offset+2+146)[0]
                assert samples==64 and crate in (0,1)
                hits[str(crate)]+=1; offset+=188
            assert offset==len(payload)
        elif name.startswith('BT'):
            magic,version,size,*v=struct.unpack('<4sHH15Q',payload)
            assert magic==b'BTS1' and version==1 and size==128
            assert v[14]-v[13]+1==v[1]
            reads[name]+=v[1]
        elif name=='BS00':
            magic,version,size,cycle,n,reserved=struct.unpack_from('<4sHHQII',payload)
            assert (magic,version,size,reserved)==(b'BSS1',1,72,0) and len(payload)==24+72*n
            for i in range(n):
                ident,enabled,priority,evaluated,rejected,evaluated_hits,rejected_hits=struct.unpack_from('<32sIi4Q',payload,24+72*i)
                ident=ident.split(b'\0')[0].decode()
                assert rejected<=evaluated and rejected_hits<=evaluated_hits
                cuts[ident].update(dict(evaluated_events=evaluated,rejected_events=rejected,evaluated_hits=evaluated_hits,rejected_hits=rejected_hits))
        elif name=='BC00':
            magic,version,size,*v=struct.unpack('<4sHH15Q',payload)
            assert magic==b'BCT1' and version==1 and size==128
            assert v[0]>last_cycle
            last_cycle=v[0]
        else: raise AssertionError('Unexpected bank '+name)
assert counts['BS00']
print(json.dumps(dict(bank_counts=counts,waveforms_by_crate=hits,acquisition_reads_by_bank=reads,cut_totals=cuts)))
''', str(recording))
    checked = json.loads(validation.stdout.strip().splitlines()[-1])
    total_hits = sum(checked['waveforms_by_crate'].values())
    total_reads = sum(checked['acquisition_reads_by_bank'].values())
    if args.collection_strategy=='periodic_readout':
        assert checked['bank_counts'].get('BD00',0)==checked['bank_counts'].get('BS00',0)
        if not args.no_timing: assert checked['bank_counts'].get('BD00',0)==checked['bank_counts'].get('BC00',0)
    assert total_hits+sum(c['rejected_hits'] for c in checked['cut_totals'].values()) == harness.numeric(variables, 'input_hits')
    if not args.no_timing: assert total_reads == harness.numeric(variables, 'input_banks')
    assert checked['bank_counts'].get('BD00',0) == harness.numeric(variables, 'output_products')
    assert checked['bank_counts']['BS00'] == harness.numeric(variables, 'diagnostic_events')
    manifest = dict(configuration=vars(config), duration_seconds=args.duration,
                    file=str(recording), bytes=recording.stat().st_size,
                    sha256=hashlib.sha256(recording.read_bytes()).hexdigest(), validation=checked,
                    waveform_format='BD00 v4; compact corrected SD/SH records; no fitting/compression; per-cut BS00 summaries',
                    timing_formats='BTnn BTS1 v1, BC00 BCT1 v1; BS00 BSS1 v1 cut summaries',
                    signal='Existing simulator pulses, Gaussian noise and jitter; random seeds 1 and 2')
    (artifact/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
