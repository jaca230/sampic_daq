#!/usr/bin/env python3
"""Self-trigger throughput scan driven by Agilent and LeCroy external pulses."""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import json
import math
from pathlib import Path
import re
import signal
import socket
import subprocess
import time

REPO = Path(__file__).resolve().parents[5]
PROJECT = REPO / 'scripts/tools/sampic_tests'
DEFAULT_CONFIG = PROJECT / 'config/dual_crate_external_generator_rate_scan.default.json'


class CaptureError(RuntimeError):
    """A failed acquisition that can be retried with a fresh crate process."""


class Generator:
    def __init__(self, config):
        self.sock = socket.create_connection((config['ip'], config['port']), timeout=5)
        self.stream = self.sock.makefile('rb')

    def write(self, command):
        self.sock.sendall((command + '\n').encode('ascii'))
        time.sleep(0.05)

    def query(self, command):
        self.write(command)
        answer = self.stream.readline(8192)
        if not answer or not answer.endswith(b'\n'):
            raise RuntimeError(f'Missing/unterminated response to {command}')
        return answer.decode('ascii').strip()

    def set_rate(self, rate):
        self.write(f'FREQ {rate:.12g}')
        actual = float(self.query('FREQ?'))
        if not math.isfinite(actual) or not math.isclose(actual, rate, rel_tol=1e-4):
            raise RuntimeError(f'Requested {rate} Hz, generator read back {actual} Hz')
        return actual

    def close(self):
        self.stream.close()
        self.sock.close()


def resolve(path):
    path = Path(path)
    return path if path.is_absolute() else REPO / path


def validate(config):
    if len(config['crates']) not in (1, 2) or len({c['name'] for c in config['crates']}) != len(config['crates']):
        raise ValueError('One or two uniquely named crates are required')
    for key in ('duration_s', 'threshold_v'):
        if not math.isfinite(config[key]) or config[key] <= 0:
            raise ValueError(f'{key} must be positive and finite')
    if not config['rates_hz'] or any(not math.isfinite(r) or r <= 0 for r in config['rates_hz']):
        raise ValueError('rates_hz must contain positive finite rates')
    if not isinstance(config.get('max_point_attempts', 6), int) or config.get('max_point_attempts', 6) < 1:
        raise ValueError('max_point_attempts must be a positive integer')
    if not math.isfinite(config.get('retry_delay_s', 2)) or config.get('retry_delay_s', 2) < 0:
        raise ValueError('retry_delay_s must be nonnegative and finite')
    if not isinstance(config['repetitions'], int) or config['repetitions'] < 1:
        raise ValueError('repetitions must be a positive integer')
    if not 1 <= config['frames_per_block'] <= 31 or config['settle_s'] < 0:
        raise ValueError('Invalid packetization or settle time')
    for crate in config['crates']:
        if not re.fullmatch(r'[A-Za-z0-9_-]+', crate['name']):
            raise ValueError('Crate names must be safe filenames')
        if crate['generator'] not in config['generators']:
            raise ValueError('Unknown generator')
        if not crate['channels'] or len(set(crate['channels'])) != len(crate['channels']):
            raise ValueError('Channel lists must be nonempty and unique')
        for identity in crate['channels']:
            feb, channel = map(int, identity.split(':'))
            if not 0 <= feb < 4 or not 0 <= channel < 64:
                raise ValueError(f'Invalid channel {identity}')
        if not resolve(crate['calibration_dir']).is_dir():
            raise ValueError(f'Missing calibration directory: {crate["calibration_dir"]}')


def command(binary, config, crate, counts):
    args = [str(binary), '--mode', 'pulser-rate', '--pulser-off',
            '--ip', crate['ip'], '--port', str(crate['port']),
            '--calibration-dir', str(resolve(crate['calibration_dir'])),
            '--threshold', str(config['threshold_v']), '--frames-per-block',
            str(config['frames_per_block']), '--events', '0',
            '--duration', str(config['duration_s']), '--quiet',
            '--channel-counts-csv', str(counts)]
    for channel in crate['channels']:
        args += ['--enable-channel', channel]
    return args


def capture(binary, config, crates, output):
    """Run one solo capture or two concurrent processes; always reap children."""
    children = []
    try:
        for crate in crates:
            name = crate['name']
            log = output / f'{name}.log'
            counts = output / f'{name}_channels.csv'
            with log.open('w') as stream:
                proc = subprocess.Popen(command(binary, config, crate, counts),
                                        cwd=REPO, stdout=stream, stderr=subprocess.STDOUT)
            children.append((crate, proc, log, counts))
        deadline = time.monotonic() + config['duration_s'] + 120
        results = []
        for crate, proc, log, counts in children:
            status = proc.wait(timeout=max(1, deadline - time.monotonic()))
            if status:
                detail = '\n'.join(log.read_text().splitlines()[-3:])
                raise CaptureError(f'{crate["name"]} exited {status}; see {log}\n{detail}')
            fields = dict(re.findall(r'^\s*([^:\n]+?)\s*:\s*([^\n]+)', log.read_text(), re.M))
            elapsed = float(fields['Elapsed'].split()[0])
            hits = int(fields['Total hits'])
            if elapsed <= 0 or hits <= 0:
                raise CaptureError(f'Invalid/empty capture; see {log}')
            # Channel CSV rates retain full precision, unlike the console summary.
            with counts.open() as stream:
                channels = list(csv.DictReader(stream))
            if sum(int(r['hits']) for r in channels) != hits:
                raise RuntimeError(f'Channel totals disagree with summary: {counts}')
            if int(fields['Decode errors']):
                raise CaptureError(f'Decode errors in {log}; capture excluded')
            selected = set(crate['channels'])
            if any(f'{r["feb"]}:{r["channel"]}' not in selected for r in channels):
                raise RuntimeError(f'Unexpected channel in {counts}')
            results.append((crate, {
                'events': int(fields['Events']), 'total_hits': hits,
                'total_bytes': int(fields['Total bytes']), 'elapsed_s': elapsed,
                'hits_per_s': sum(float(r['hits_per_s']) for r in channels),
                'decode_errors': int(fields['Decode errors']),
                'log': str(log), 'channel_counts': str(counts),
            }))
        return results
    finally:
        for _, proc, _, _ in children:
            if proc.poll() is None:
                proc.send_signal(signal.SIGINT)
        for _, proc, _, _ in children:
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


def capture_with_retries(binary, config, crates, point, report):
    attempts = config.get('max_point_attempts', 6)
    delay = config.get('retry_delay_s', 2)
    for attempt in range(1, attempts + 1):
        directory = point / f'attempt_{attempt:02d}'
        directory.mkdir()
        report(f'  acquisition attempt {attempt}/{attempts}; logs: {directory}')
        try:
            return capture(binary, config, crates, directory), attempt
        except (CaptureError, subprocess.TimeoutExpired) as exc:
            (directory / 'failure.json').write_text(json.dumps({
                'attempt': attempt, 'error': str(exc),
                'time': datetime.now().isoformat()}, indent=2) + '\n')
            report(f'  FAILED attempt {attempt}/{attempts}: {exc}')
            if attempt == attempts:
                raise CaptureError(f'All {attempts} attempts failed; see {point}') from exc
            report(f'  retrying with fresh crate connections in {delay:g} s')
            time.sleep(delay)


def scan(config, binary, output):
    # Refuse another local owner of the crate connections.
    processes = subprocess.check_output(['ps', '-eo', 'comm='], text=True).splitlines()
    if any(p.strip() in ('sampic_frontend', 'sampic_deadtime', 'sampic_deadtime_scan') for p in processes):
        raise RuntimeError('Stop SAMPIC frontends/diagnostic acquisitions before scanning')
    output.mkdir(parents=True, exist_ok=False)
    def report(message):
        line = f'[{datetime.now().strftime("%H:%M:%S")}] {message}'
        print(line, flush=True)
        with (output / 'scan.log').open('a') as log:
            log.write(line + '\n')

    report(f'Results directory: {output}')
    (output / 'scan_config.json').write_text(json.dumps(config, indent=2) + '\n')
    devices, originals, metadata = {}, {}, {'generators': {}, 'restoration': {}}
    try:
        used_generators = {crate['generator'] for crate in config['crates']}
        for name, cfg in config['generators'].items():
            if name not in used_generators:
                continue
            device = Generator(cfg)
            devices[name] = device
            ident = device.query('*IDN?')
            if cfg['id_contains'] not in ident:
                raise RuntimeError(f'{name}: unexpected instrument {ident}')
            originals[name] = float(device.query('FREQ?'))
            if not math.isfinite(originals[name]) or originals[name] <= 0:
                raise RuntimeError(f'{name}: invalid original frequency')
            state = {'id': ident, 'original_frequency_hz': originals[name]}
            if name == 'agilent':
                for q in ('OUTPut?', 'FUNCtion?', 'VOLTAGE?', 'VOLTage:OFFSet?'):
                    state[q] = device.query(q)
                if state['OUTPut?'] not in ('1', 'ON') or state['FUNCtion?'] != 'PULS':
                    raise RuntimeError('Agilent must already be enabled in pulse mode')
                if device.query('BURSt:STATe?') not in ('0', 'OFF'):
                    raise RuntimeError('Agilent burst mode must be off for one pulse per period')
            elif name == 'lecroy':
                for q in ('TRMD?', 'A:DISA?', 'B:DISA?', 'A:DBL?', 'B:DBL?',
                          'A:AMP?', 'B:AMP?', 'A:WID?', 'B:WID?'):
                    state[q] = device.query(q)
                if any(state[q] not in ('OFF', '0') for q in ('A:DISA?', 'B:DISA?', 'A:DBL?', 'B:DBL?')):
                    raise RuntimeError('LeCroy A/B must be enabled with double-pulse mode off')
                if state['TRMD?'].upper() not in ('NORMAL', 'NORM', 'AUTO', 'AUT', 'CONT', 'CONTINUOUS'):
                    raise RuntimeError(f'LeCroy needs free-running trigger mode: {state["TRMD?"]}')
            metadata['generators'][name] = state
            report(f'{name}: {ident}; starting frequency {originals[name]:g} Hz')
        with (output / 'results.csv').open('w', newline='') as stream:
            writer = None
            for rate in config['rates_hz']:
                actual = {name: device.set_rate(rate) for name, device in devices.items()}
                report('Generator rate readback: ' + ', '.join(f'{name}={value:g} Hz' for name, value in actual.items()))
                time.sleep(config['settle_s'])
                for rep in range(config['repetitions']):
                    captures = [('solo', [crate]) for crate in config['crates']]
                    if len(config['crates']) == 2:
                        captures.append(('parallel', config['crates']))
                    for condition, crates in captures:
                        names = '_'.join(c['name'] for c in crates)
                        point = output / f'rate_{rate:g}_rep_{rep+1}_{condition}_{names}'
                        point.mkdir()
                        report(f'{rate:g} Hz, rep {rep+1}/{config["repetitions"]}, {condition}: {names}')
                        captured, attempt = capture_with_retries(binary, config, crates, point, report)
                        after = {name: float(device.query('FREQ?')) for name, device in devices.items()}
                        (point / 'generator_readback.json').write_text(json.dumps(after, indent=2) + '\n')
                        if any(not math.isclose(after[name], value, rel_tol=1e-4)
                               for name, value in actual.items()):
                            raise RuntimeError(f'Generator frequency changed during capture: {after}; see {point}')
                        for crate, stats in captured:
                            offered = len(crate['channels']) * actual[crate['generator']]
                            row = {'condition': condition, 'crate': crate['name'],
                                   'repetition': rep+1, 'nominal_rate_hz': rate,
                                   'configured_rate_hz': actual[crate['generator']],
                                   'channel_count': len(crate['channels']), 'attempt': attempt,
                                   'expected_hits_per_s': offered, **stats,
                                   'capture_efficiency': stats['hits_per_s'] / offered}
                            if row['capture_efficiency'] > 1.10:
                                (point / 'invalid_result.json').write_text(json.dumps(row, indent=2) + '\n')
                                raise RuntimeError(f'{crate["name"]}: observed rate exceeds offered rate by >10%; '
                                                   f'check cabling, pulse shape, and generator control. See {point}')
                            if writer is None:
                                writer = csv.DictWriter(stream, fieldnames=list(row))
                                writer.writeheader()
                            writer.writerow(row)
                            stream.flush()
                            report(f'  {crate["name"]}: {stats.get("total_hits", 0):,} hits, '
                                   f'{stats["hits_per_s"]:,.1f} hits/s / {offered:,.1f} offered, '
                                   f'efficiency={100*row["capture_efficiency"]:.2f}%, '
                                   f'decode errors={stats.get("decode_errors", 0)}')
    finally:
        failures = []
        for name, device in devices.items():
            try:
                if name in originals:
                    try:
                        restored = device.set_rate(originals[name])
                    except Exception:
                        # Some instrument bridges allow only one TCP client and
                        # can drop an idle connection during a long acquisition.
                        device.close()
                        device = Generator(config['generators'][name])
                        restored = device.set_rate(originals[name])
                    metadata['restoration'][name] = restored
                    report(f'{name}: restored frequency to {restored:g} Hz')
            except Exception as exc:
                failures.append(f'{name}: {exc}')
                metadata['restoration'][name] = str(exc)
            finally:
                device.close()
        (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
        if failures:
            raise RuntimeError('Generator frequency restoration failed: ' + '; '.join(failures))


def plot(output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    with (output / 'results.csv').open() as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError('No results to plot')
    fig, ax = plt.subplots(figsize=(10, 6))
    crate_names = sorted({row['crate'] for row in rows})
    for series in crate_names + ['parallel']:
        groups = {}
        for row in rows:
            matches = (row['condition'] == 'parallel' if series == 'parallel'
                       else row['condition'] == 'solo' and row['crate'] == series)
            if matches:
                groups.setdefault((float(row['nominal_rate_hz']), row['repetition']), []).append(row)
        by_rate = {}
        for (rate, rep), group in groups.items():
            if len(group) != (2 if series == 'parallel' else 1):
                continue
            offered = sum(float(r['expected_hits_per_s']) for r in group)
            observed = sum(float(r['hits_per_s']) for r in group)
            by_rate.setdefault(rate, []).append((offered, observed / offered))
        points = []
        for samples in by_rate.values():
            x = sum(x for x, y in samples) / len(samples)
            y = sum(y for x, y in samples) / len(samples)
            low, high = min(y for x, y in samples), max(y for x, y in samples)
            points.append((x, y, low, high))
        if points:
            points.sort()
            ax.errorbar([x/1000 for x,y,lo,hi in points],
                        [100*y for x,y,lo,hi in points],
                        yerr=[[100*(y-lo) for x,y,lo,hi in points],
                              [100*(hi-y) for x,y,lo,hi in points]], fmt='o-', capsize=3,
                        label='Two crates combined' if series == 'parallel' else f'{series} solo')
    for threshold in (95, 99):
        ax.axhline(threshold, color='gray', linestyle=':', linewidth=0.8)
    ax.set(xlabel='Offered hit rate (k hits/s)', ylabel='Observed / offered (%)',
           title='External pulses, SAMPIC self-trigger capture efficiency', ylim=(0, 105))
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    for extension in ('pdf', 'png', 'svg'):
        fig.savefig(output / f'efficiency_vs_offered_hit_rate.{extension}')
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--rates-hz', type=float, nargs='+', help='Override frequency grid in Hz')
    parser.add_argument('--duration', type=float, help='Override seconds per capture')
    parser.add_argument('--repetitions', type=int, help='Override captures per frequency')
    parser.add_argument('--retries', type=int, help='Retries after a failed capture (default 5)')
    parser.add_argument('--retry-delay', type=float, help='Seconds between attempts (default 2)')
    parser.add_argument('--apply', action='store_true', help='Set generator rates and acquire; otherwise dry-run')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--plot', type=Path, help='Plot an existing results directory; no hardware access')
    args = parser.parse_args()
    if args.plot:
        plot(args.plot)
        return
    config = json.loads(args.config.read_text())
    if args.rates_hz is not None:
        config['rates_hz'] = args.rates_hz
    if args.duration is not None:
        config['duration_s'] = args.duration
    if args.repetitions is not None:
        config['repetitions'] = args.repetitions
    if args.retries is not None:
        config['max_point_attempts'] = args.retries + 1
    if args.retry_delay is not None:
        config['retry_delay_s'] = args.retry_delay
    validate(config)
    binary = PROJECT / 'build/bin/sampic_deadtime_scan'
    if not binary.is_file():
        raise ValueError(f'Build {binary} first')
    output = resolve(args.output_dir or Path(config['output_root']) / datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    if not args.apply:
        print(json.dumps({'config': config, 'output': str(output),
                      'example_command': command(binary, config, config['crates'][0], output/'channels.csv')}, indent=2))
    if not args.apply:
        print('Dry run: no connections or files created. Add --apply to acquire.')
        return
    scan(config, binary, output)
    print(f'Results: {output}\nPlot with: python3 {Path(__file__)} --plot {output}')


if __name__ == '__main__':
    def interrupted(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    try:
        main()
    except (Exception, KeyboardInterrupt) as exc:
        raise SystemExit(f'Scan stopped: {exc}') from exc
