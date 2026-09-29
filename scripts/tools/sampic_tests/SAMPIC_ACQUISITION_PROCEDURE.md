# SAMPIC batching acquisition procedure

This is the discussion-facing map of the hardware procedure. The scan grid,
retry bookkeeping, CSV/JSON export, and report generation are intentionally
outside this document.

## The three code locations that matter

1. **Vendor session and low-level settings**

   `src/tests/external_trigger_probe/sampic_session.h`, class
   `SimpleSession`.

   This class owns the crate connection, vendor parameter structures, and
   vendor event buffers. Its public methods are the complete hardware-facing
   surface used by the batching scan:

   - `set_sampling_rate()`
   - `set_packetization()`
   - `enable_all_channels()` / `enable_channels()`
   - `enable_l2_external_gate()`
   - `enable_plain_self_trigger()`
   - `enable_plain_external_trigger()`
   - `start_run()`, `read_event()` / `read_raw_event()`, and `stop_run()`

   Constructor initialization applies the common settings: defaults,
   calibration, channel trigger mode, external-trigger type/edge/level and
   synchronization, external-trigger counter mode, and thresholds. Search for
   `configure_base()` and `configure_defaults()` within the class.

2. **Scheme selection for a scan point**

   `src/tests/external_trigger_probe/acquisition_sequencer.h`, static method
   `AcquisitionSequencer::ConfigureSession()`.

   This deliberately small function is the authoritative scheme switch:

   | Scheme | Enabled channels | Channel trigger | L2 build | External gate |
   |---|---|---|---|---|
   | `external` | all 256 | external | off | off |
   | `self_trigger` | all 256 | self | off | off (counter retained for comparison) |
   | `l2_external_gate` | all 256 | self | on, FEB OR | on |

   The packet settings are applied immediately before this switch in
   `run_persistent_batching_scan()`. They are reapplied at every point because
   the vendor `StopRun()` resets them.

3. **One actual acquisition**

   `src/tests/external_trigger_probe/acquisition_sequencer.h`, class
   `AcquisitionSequencer` and method `Capture()`.

   This is where one point runs. Its hardware sequence is:

   1. Inhibit Lecroy outputs.
   2. Program the requested trigger frequency and query it back.
   3. Allocate the large vendor event buffers, result vectors, and (in
      pipelined mode) the complete bounded raw-frame queue.
   4. Reset vendor transport counters, when that API is available.
   5. Call `SAMPIC256CH_StartRun()` (through `SimpleSession::start_run()`).
   6. In pipelined mode, start both the dedicated receiver and decoder threads.
      Wait until the decoder is ready on the empty queue and the receiver has
      entered the vendor read path.
   7. Enable Lecroy outputs; acquisition is now live and reading begins
      immediately.
   8. Read until the point duration or event limit is reached. In pipelined
      mode the control thread remains separate: the receiver thread exclusively
      calls the vendor read API and the decoder thread consumes its queue.
   9. Inhibit Lecroy at the cutoff. For L2 gating, output B (external gate) is
      inhibited before A (analog pulses).
   10. Continue reading until no event arrives for `drain_quiet_ms`, bounded by
      `drain_timeout_s`. These events are marked `during_drain`.
   11. Read transport counters and call `SAMPIC256CH_StopRun()`.
   12. Analyze and export the already-recorded data.

   Exception cleanup also inhibits Lecroy before stopping the SAMPIC run.

## Persistent-scan ordering

`persistent_batching_scan.h` contains `run_persistent_batching_scan()`, which
owns one crate connection across points. For
each point it performs exactly:

```text
construct point options
  -> ensure crate session exists
  -> apply packetization
  -> apply scheme settings if the session/scheme changed
  -> capture_batching_point()
  -> write manifest status
```

On a point failure, both instrument connections are refreshed before retrying.
No crate or Lecroy hardware is contacted by report-generation code.

## Values versus implementation

The values under discussion normally come from:

- `config/external_trigger_batching_scan.default.json`: rates, batching grid,
  scheme list, duration, drain timing, and L2 gate lengths.
- `config/double_pulse_deadtime_scan.default.json`: crate endpoint,
  calibration path, sampling rate, trigger electrical settings, thresholds,
  read retry policy, and run-start retry policy.

The first file chooses *what points to run*. The second describes *how the
hardware is connected and electrically configured*. The C++ locations above
define *how those values are applied and in what order*.
