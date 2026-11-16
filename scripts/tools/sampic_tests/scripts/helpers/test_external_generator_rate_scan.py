"""Hardware-free orchestration checks: python3 -m unittest discover -s <this directory>."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('scan', Path(__file__).with_name('dual_crate_external_generator_rate_scan.py'))
scan = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scan)


class FakeGenerator:
    instances = []

    def __init__(self, config):
        self.name = config['id_contains']
        self.rate = 1000 if self.name == '33250A' else 10000
        self.initial = self.rate
        self.closed = False
        self.instances.append(self)

    def query(self, command):
        return {'*IDN?': self.name, 'FREQ?': str(self.rate), 'OUTPut?': '1',
                'FUNCtion?': 'PULS', 'VOLTAGE?': '1', 'VOLTage:OFFSet?': '0',
                'BURSt:STATe?': '0', 'TRMD?': 'NORMAL'}.get(command, 'OFF')

    def set_rate(self, rate):
        self.rate = rate
        return rate

    def close(self):
        self.closed = True


class ScanTests(unittest.TestCase):
    def setUp(self):
        FakeGenerator.instances = []
        self.config = json.loads(scan.DEFAULT_CONFIG.read_text())
        self.config.update(rates_hz=[250], repetitions=1, settle_s=0)

    def fake_capture(self, binary, config, crates, output):
        self.calls.append([c['name'] for c in crates])
        return [(c, {'hits_per_s': len(c['channels']) * 250, 'total_hits': 100}) for c in crates]

    def test_solo_parallel_and_channel_normalization(self):
        self.calls = []
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'Generator', FakeGenerator), \
             patch.object(scan.subprocess, 'check_output', return_value=''), \
             patch.object(scan, 'capture', side_effect=self.fake_capture):
            output = Path(tmp) / 'scan'
            scan.scan(self.config, Path('fake'), output)
            import csv
            with (output / 'results.csv').open() as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), 4)
            self.assertEqual(self.calls, [['crate_n1'], ['crate_n2'], ['crate_n1', 'crate_n2']])
            self.assertEqual([float(r['expected_hits_per_s']) for r in rows], [1500, 2000, 1500, 2000])
            self.assertTrue(all(float(r['capture_efficiency']) == 1 for r in rows))
        self.assertTrue(all(d.rate == d.initial and d.closed for d in FakeGenerator.instances))

    def test_agilent_solo_never_connects_lecroy_or_runs_parallel(self):
        self.config['crates'] = self.config['crates'][:1]
        # Even with an unused generator in the config, connect only to Agilent.
        scan.validate(self.config)
        self.calls = []
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'Generator', FakeGenerator), \
             patch.object(scan.subprocess, 'check_output', return_value=''), \
             patch.object(scan, 'capture', side_effect=self.fake_capture):
            scan.scan(self.config, Path('fake'), Path(tmp) / 'scan')
        self.assertEqual(self.calls, [['crate_n1']])
        self.assertEqual([d.name for d in FakeGenerator.instances], ['33250A'])
        self.assertTrue(all(d.rate == d.initial and d.closed for d in FakeGenerator.instances))

    def test_retry_preserves_failed_attempt_and_returns_success(self):
        self.config.update(max_point_attempts=3, retry_delay_s=0)
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'capture',
                side_effect=[scan.CaptureError('SetDefaultParameters failed (-28)'), [('success', {})]]) as capture:
            result, attempt = scan.capture_with_retries(Path('fake'), self.config, [], Path(tmp), lambda msg: None)
            self.assertEqual(attempt, 2)
            self.assertEqual(result, [('success', {})])
            self.assertTrue((Path(tmp) / 'attempt_01/failure.json').is_file())
            self.assertEqual(capture.call_count, 2)

    def test_retry_exhaustion_is_bounded(self):
        self.config.update(max_point_attempts=2, retry_delay_s=0)
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'capture',
                side_effect=scan.CaptureError('transport failure')) as capture:
            with self.assertRaisesRegex(scan.CaptureError, 'All 2 attempts failed'):
                scan.capture_with_retries(Path('fake'), self.config, [], Path(tmp), lambda msg: None)
            self.assertEqual(capture.call_count, 2)
            self.assertTrue((Path(tmp) / 'attempt_02/failure.json').is_file())

    def test_failure_restores_frequencies(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'Generator', FakeGenerator), \
             patch.object(scan.subprocess, 'check_output', return_value=''), \
             patch.object(scan, 'capture', side_effect=RuntimeError('acquisition failed')):
            with self.assertRaisesRegex(RuntimeError, 'acquisition failed'):
                scan.scan(self.config, Path('fake'), Path(tmp) / 'scan')
        self.assertTrue(all(d.rate == d.initial and d.closed for d in FakeGenerator.instances))

    def test_impossible_efficiency_aborts_and_restores(self):
        def excessive_rate(binary, config, crates, output):
            return [(c, {'hits_per_s': 1000000, 'total_hits': 100}) for c in crates]
        with tempfile.TemporaryDirectory() as tmp, patch.object(scan, 'Generator', FakeGenerator), \
             patch.object(scan.subprocess, 'check_output', return_value=''), \
             patch.object(scan, 'capture', side_effect=excessive_rate):
            with self.assertRaisesRegex(RuntimeError, 'exceeds offered'):
                scan.scan(self.config, Path('fake'), Path(tmp) / 'scan')
        self.assertTrue(all(d.rate == d.initial and d.closed for d in FakeGenerator.instances))

    def test_internal_pulser_disabled_and_mapping_selected(self):
        crate = self.config['crates'][1]
        cmd = scan.command(Path('binary'), self.config, crate, Path('channels.csv'))
        self.assertIn('--pulser-off', cmd)
        self.assertEqual([cmd[i+1] for i, a in enumerate(cmd) if a == '--enable-channel'], crate['channels'])
        self.assertNotIn('3:51', crate['channels'])

    def test_invalid_channels_rejected(self):
        self.config['crates'][0]['channels'] = ['3:64']
        with self.assertRaises(ValueError):
            scan.validate(self.config)


if __name__ == '__main__':
    unittest.main()
