import csv
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "threshold_scan", ROOT / "scripts/tools/threshold_scan/scan.py"
)
scan = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scan)


class FakeRunner:
    def __init__(self, mode="quiet"):
        self.mode = mode
        self.calls = []
        self.fail_once = False

    def __call__(self, operation, directory, **parameters):
        directory.mkdir(parents=True, exist_ok=False)
        if operation == "discover":
            return {"schema_version": 1, "board_count": 1, "paths": [2]}
        if operation == "quiesce":
            return {"cleanup_verified": True}
        keys = [f"{f}:{c}" for f, c in parameters["channels"]]
        self.calls.append(keys)
        if self.mode == "fail" or self.fail_once:
            self.fail_once = False
            raise scan.MeasurementError("UDP timeout")
        limited = self.mode == "hot" and "0:0" in keys
        counts = {k: (1000 if k == "0:0" and limited else 0) for k in keys}
        return {
            "schema_version": 1,
            "status": "rate_limited" if limited else "complete",
            "cleanup_verified": True,
            "board_paths": [2],
            "elapsed_s": 0.2,
            "counts": counts,
            "total_hits": sum(counts.values()),
            "frame_bytes": 100,
            "applied_thresholds_v": {k: parameters["threshold_v"] for k in keys},
        }


class ThresholdTests(unittest.TestCase):
    def config(self):
        c = scan.load_config(scan.EXAMPLE)
        c.update(
            channels=["0:0", "0:1", "0:2", "0:3"],
            threshold_min_v=0.1,
            threshold_max_v=0.3,
            threshold_step_v=0.1,
            retry_delay_s=0,
        )
        return c

    def run_scan(self, c, runner, tmp):
        out = Path(tmp) / "scan"
        result = scan.Scan(c, runner, out).run()
        with (out / "results.csv").open() as f:
            rows = list(csv.DictReader(f))
        return result, rows, out

    def test_all_channels_together_when_quiet(self):
        with tempfile.TemporaryDirectory() as tmp:
            runner = FakeRunner()
            meta, rows, out = self.run_scan(self.config(), runner, tmp)
            self.assertEqual(len(runner.calls), 3)
            self.assertTrue(all(len(c) == 4 for c in runner.calls))
            self.assertEqual(len(rows), 12)
            self.assertTrue(
                all(r["status"] == "complete" and r["hits"] == "0" for r in rows)
            )
            self.assertTrue(all(float(r["zero_hit_upper_95_hz"]) > 0 for r in rows))
            self.assertEqual(meta["status"], "complete")

    def test_hot_channel_splits_and_blocks_only_that_channel(self):
        with tempfile.TemporaryDirectory() as tmp:
            runner = FakeRunner("hot")
            meta, rows, out = self.run_scan(self.config(), runner, tmp)
            hot = [r for r in rows if r["channel"] == "0"]
            self.assertEqual(
                [r["status"] for r in hot],
                ["rate_limited", "skipped_guard", "skipped_guard"],
            )
            self.assertTrue(all(r["observed_rate_hz"] == "" for r in hot[1:]))
            self.assertTrue(
                all(r["status"] == "complete" for r in rows if r["channel"] != "0")
            )
            self.assertIn(["0:0"], runner.calls)
            self.assertEqual(meta["blocked_channels"], {"0:0": "rate_limited"})

    def test_retry_logs_attempt_without_false_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            runner = FakeRunner()
            runner.fail_once = True
            meta, rows, out = self.run_scan(self.config(), runner, tmp)
            self.assertEqual(rows[0]["attempt"], "2")
            self.assertTrue((out / "failures.jsonl").exists())
            self.assertEqual(len(rows), 12)

    def test_failure_budget_aborts_and_retains_metadata(self):
        c = self.config()
        c["max_consecutive_failures"] = 3
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "scan"
            with self.assertRaises(scan.SafetyError):
                scan.Scan(c, FakeRunner("fail"), out).run()
            self.assertEqual(
                json.loads((out / "metadata.json").read_text())["status"], "failed"
            )
            self.assertTrue((out / "results.csv").exists())

    def test_falling_grid_order(self):
        c = self.config()
        c.update(
            edge="falling",
            threshold_min_v=-0.2,
            threshold_max_v=0,
            threshold_step_v=0.1,
        )
        self.assertEqual(scan.thresholds(c), [-0.2, -0.1, 0])

    def test_unknown_config_and_bad_channel_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "config.json"
            c = json.loads(scan.EXAMPLE.read_text())
            c["duraton_s"] = 1
            p.write_text(json.dumps(c))
            with self.assertRaises(ValueError):
                scan.load_config(p)
        for key in ("0:64", "4:0", "0:-1", "0:1;rm", "foo"):
            with self.assertRaises(ValueError):
                scan.parse_channel(key)

    def test_bad_measurement_cannot_be_accepted(self):
        with self.assertRaises(scan.SafetyError):
            scan.validate_measurement({"cleanup_verified": False}, ["0:0"])
        good = {
            "schema_version": 1,
            "cleanup_verified": True,
            "status": "complete",
            "elapsed_s": 0.2,
            "counts": {"0:0": 2},
            "total_hits": 1,
            "applied_thresholds_v": {"0:0": 0.1},
        }
        with self.assertRaises(scan.MeasurementError):
            scan.validate_measurement(good, ["0:0"])

    def fake_worker(self, tmp, fail_recovery=False):
        p = Path(tmp) / "worker.py"
        p.write_text("""#!/usr/bin/env python3
import json,sys,time
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
if r['operation']=='measure':time.sleep(10)
else:
 Path(sys.argv[2]).write_text(json.dumps({'schema_version':1,'cleanup_verified':%s}))
 sys.exit(%s)
""" % ("False" if fail_recovery else "True", "1" if fail_recovery else "0"))
        p.chmod(0o755)
        return p

    def test_worker_timeout_runs_separate_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            c = self.config()
            c["worker_timeout_s"] = 0.1
            c["recovery_timeout_s"] = 2
            runner = scan.WorkerRunner(c, self.fake_worker(tmp))
            out = Path(tmp) / "attempt"
            with self.assertRaises(scan.MeasurementError):
                runner("measure", out, channels=[[0, 0]], threshold_v=0.1)
            self.assertTrue(
                json.loads((out / "recovery/result.json").read_text())[
                    "cleanup_verified"
                ]
            )

    def test_recovery_failure_is_fatal_not_retried(self):
        with tempfile.TemporaryDirectory() as tmp:
            c = self.config()
            c["worker_timeout_s"] = 0.1
            c["recovery_timeout_s"] = 2
            runner = scan.WorkerRunner(c, self.fake_worker(tmp, True))
            with self.assertRaises(scan.SafetyError):
                runner(
                    "measure", Path(tmp) / "attempt", channels=[[0, 0]], threshold_v=0.1
                )

    def test_interrupt_recovery_then_propagates(self):
        runner = scan.WorkerRunner(self.config(), Path("unused"))
        with patch.object(
            runner, "raw", side_effect=[KeyboardInterrupt(), {"cleanup_verified": True}]
        ) as raw:
            with self.assertRaises(KeyboardInterrupt):
                runner("measure", Path("attempt"))
            self.assertEqual(
                [c.args[0] for c in raw.call_args_list], ["measure", "quiesce"]
            )

    def test_multicrate_config_channel_selections_and_duplicates(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            config = {
                "crates": [
                    {
                        "name": "n1",
                        "crate": self.config()["crate"],
                        "channels": ["0:1", "0:3"],
                    },
                    {
                        "name": "n2",
                        "crate": {**self.config()["crate"], "port": 27014},
                        "channels": ["0:4"],
                    },
                ]
            }
            path.write_text(json.dumps(config))
            loaded = scan.load_configs(path)
            self.assertEqual([c["channels"] for c in loaded], [["0:1", "0:3"], ["0:4"]])
            config["crates"][1]["crate"] = config["crates"][0]["crate"]
            path.write_text(json.dumps(config))
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                scan.load_configs(path)

    def test_multicrate_simulation_outputs_separate_reports(self):
        import subprocess

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            config = self.config()
            crate = config.pop("crate")
            config["threshold_max_v"] = config["threshold_min_v"]
            config["crates"] = [
                {"name": "n1", "crate": crate, "channels": ["0:1"]},
                {"name": "n2", "crate": {**crate, "port": 27014}, "channels": ["1:2"]},
            ]
            path.write_text(json.dumps(config))
            output = Path(tmp) / "output"
            result = subprocess.run(
                [
                    sys.executable,
                    str(scan.__file__),
                    "--config",
                    str(path),
                    "--simulate",
                    "--no-plots",
                    "--output-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                timeout=15,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(
                json.loads((output / "run.json").read_text())["status"], "complete"
            )
            for name, channel in [("n1", "1"), ("n2", "2")]:
                with (output / name / "results.csv").open() as f:
                    rows = list(csv.DictReader(f))
                self.assertEqual(len(rows), 1)
                self.assertEqual(rows[0]["channel"], channel)
                self.assertIn(f"[{name}]", result.stdout)

    def test_multicrate_interrupt_recovers_each_active_worker(self):
        import subprocess
        import signal
        import time

        with tempfile.TemporaryDirectory() as tmp:
            worker = Path(tmp) / "worker.py"
            worker.write_text("""#!/usr/bin/env python3
import json, sys, time
from pathlib import Path
request = json.loads(Path(sys.argv[1]).read_text())
op = request['operation']
if op == 'discover':
    result = {'schema_version': 1, 'board_count': 1, 'paths': [0]}
elif op == 'quiesce':
    result = {'schema_version': 1, 'cleanup_verified': True}
else:
    Path(sys.argv[2] + '.active').write_text('active')
    time.sleep(60)
    sys.exit(1)
Path(sys.argv[2]).write_text(json.dumps(result))
""")
            worker.chmod(0o755)
            path = Path(tmp) / "config.json"
            config = self.config()
            crate = config.pop("crate")
            config["crates"] = [
                {"name": "n1", "crate": {**crate, "ip": "127.0.0.241"}},
                {"name": "n2", "crate": {**crate, "ip": "127.0.0.242"}},
            ]
            path.write_text(json.dumps(config))
            output = Path(tmp) / "output"
            # This harness removes the environment-dependent frontend process check;
            # the executable is a local fake and never opens a network connection.
            harness = "import sys,contextlib;sys.path.insert(0,sys.argv.pop(1));import scan;scan.CrateLock=lambda c:contextlib.nullcontext();scan.main()"
            proc = subprocess.Popen(
                [
                    sys.executable,
                    "-c",
                    harness,
                    str(Path(scan.__file__).parent),
                    "--config",
                    str(path),
                    "--apply",
                    "--worker",
                    str(worker),
                    "--no-plots",
                    "--output-dir",
                    str(output),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                deadline = time.monotonic() + 10
                while len(list(output.glob("*/attempts/*/result.json.active"))) != 2:
                    if proc.poll() is not None or time.monotonic() > deadline:
                        self.fail("Both fake workers did not enter acquisition")
                    time.sleep(0.05)
                proc.send_signal(signal.SIGINT)
                log, _ = proc.communicate(timeout=15)
                self.assertNotEqual(proc.returncode, 0)
                self.assertEqual(
                    json.loads((output / "run.json").read_text())["status"],
                    "interrupted",
                )
                for name in ("n1", "n2"):
                    self.assertEqual(
                        json.loads((output / name / "metadata.json").read_text())[
                            "status"
                        ],
                        "interrupted",
                        log,
                    )
                    recovery = list(
                        (output / name).glob("attempts/*/recovery/result.json")
                    )
                    self.assertEqual(len(recovery), 1, log)
                    self.assertTrue(
                        json.loads(recovery[0].read_text())["cleanup_verified"]
                    )
            finally:
                if proc.poll() is None:
                    proc.send_signal(signal.SIGINT)
                    proc.communicate(timeout=15)

    def test_screen_runs_scanner_directly_without_interactive_prompt(self):
        import argparse

        args = argparse.Namespace(
            apply=False,
            simulate=True,
            screen="threshold_test",
            config=scan.EXAMPLE,
            worker=Path("fake_worker"),
            output_dir=None,
            no_plots=True,
        )
        with patch.object(
            scan.shutil, "which", return_value="/usr/bin/screen"
        ), patch.object(scan.subprocess, "run") as run:
            run.return_value.stdout = "No Sockets found."
            scan.launch_screen(args)
        command = run.call_args_list[-1].args[0]
        self.assertEqual(command[:3], ["screen", "-dmS", "threshold_test"])
        self.assertEqual(command[3], sys.executable)
        self.assertNotIn("bash", command)
        self.assertNotIn("read", " ".join(command))

    def test_recovery_retries_before_restarting_measurement(self):
        runner = scan.WorkerRunner(self.config(), Path("unused"))
        with patch.object(
            runner,
            "raw",
            side_effect=[
                scan.MeasurementError("capture failed"),
                scan.MeasurementError("disable timeout"),
                {"cleanup_verified": True},
            ],
        ) as raw:
            with self.assertRaisesRegex(scan.MeasurementError, "capture failed"):
                runner("measure", Path("attempt"))
            self.assertEqual(
                [c.args[0] for c in raw.call_args_list],
                ["measure", "quiesce", "quiesce"],
            )
            self.assertEqual(raw.call_args_list[-1].args[1], Path("attempt/recovery_2"))

    def test_exhausted_errors_do_not_split_channel_groups(self):
        c = self.config()
        c["max_attempts"] = 2
        c["max_consecutive_failures"] = 10
        runner = FakeRunner("fail")
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "scan"
            with self.assertRaisesRegex(scan.SafetyError, "without splitting"):
                scan.Scan(c, runner, output).run()
            self.assertEqual(runner.calls, [c["channels"], c["channels"]])
            with (output / "results.csv").open() as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), 4)
            self.assertTrue(
                all(
                    r["status"] == "failed" and r["observed_rate_hz"] == ""
                    for r in rows
                )
            )

    def test_confirmed_same_connection_cleanup_skips_reconnect_recovery(self):
        runner = scan.WorkerRunner(self.config(), Path("unused"))
        with patch.object(
            runner,
            "raw",
            side_effect=scan.MeasurementError(
                "read failed but local cleanup succeeded", cleanup_verified=True
            ),
        ) as raw:
            with self.assertRaises(scan.MeasurementError):
                runner("measure", Path("attempt"))
            self.assertEqual(raw.call_count, 1)

    def test_reconnect_fallback_waits_first(self):
        c = self.config()
        c["retry_delay_s"] = 1.5
        runner = scan.WorkerRunner(c, Path("unused"))
        events = []

        def raw(op, *args, **kwargs):
            events.append(op)
            if op == "measure":
                raise scan.MeasurementError("same-connection cleanup exhausted")
            return {"cleanup_verified": True}

        with patch.object(runner, "raw", side_effect=raw), patch.object(
            scan.time, "sleep", side_effect=lambda delay: events.append(delay)
        ):
            with self.assertRaises(scan.MeasurementError):
                runner("measure", Path("attempt"))
        self.assertEqual(events, ["measure", 1.5, "quiesce"])


if __name__ == "__main__":
    unittest.main()
