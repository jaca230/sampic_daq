# SAMPIC event builder

This project builds time-coincident events from independent SAMPIC crate
streams. Crate frontends remain one-process-per-crate and publish vendor-mode
MIDAS banks. The event builder merges those banks by their shared hardware
clock, applies optional event selection, and optionally processes waveforms.

## Project boundaries

- `../template_fitter`: sibling data-agnostic waveform-fitting project. The first backend
  is `cpu-least-squares`; CUDA fitting can implement the same interface.
- `../residual_compression`: sibling data-agnostic signed-integer-coding project. The
  first backend is `cpu-rice-golomb`; GPU ANS belongs here as another codec.
- `include/sampic_event_builder` and `src`: replaceable pipeline stages,
  time grouping, selection, output-product serialization, and telemetry.
- `benchmarks` and `tests/support`: synthetic inputs and timing tools that are
  not linked into the production event builder.

The fixed pipeline order is:

```text
input -> build events -> select -> fit -> compress -> finalize
```

`EventBuilderPipeline` owns one implementation of each abstract stage type and
can replace any stage without changing the others. Concrete stage configuration
lives beside that implementation; there is no monolithic configuration struct.

## Multi-frontend collection

Each crate frontend remains independently useful and retains all of its
existing collection modes. For centralized event building, run each crate in
`vendor_raw_passthrough` mode. Its `SRnn` bank contains the crate number in the
bank suffix and preserves the ordered integer ADC samples needed by fitting and
lossless compression. The event-builder frontend subscribes to every such bank,
decodes it into the common `Hit` model, and then applies one replaceable
`EventBuildingStage` across all crate streams.

The event-builder frontend provides these collection strategies through
`/Equipment/SAMPIC Builder/Settings/Collection strategy`:

- `time_grouping` (or `default`) clusters hardware timestamps into relative
  windows. Per-crate watermarks prevent a fast stream from closing an event
  before a slower stream can contribute.
- `fixed_time_bins` assigns all streams to common absolute hardware-time bin
  boundaries. It can retain empty bins and uses the slowest expected crate as
  its completion watermark.
- `periodic_readout` drains all hits received from every stream on a software
  steady-clock period.

Set `/Equipment/SAMPIC Builder/Settings/Expected crates` to the number of
independent crate frontends. Strategy-specific settings live under the
`Time grouping`, `Fixed time bins`, and `Periodic readout` subdirectories.

`external_gated_trigger` is not exposed at this layer yet: the current `SRnn`
input contract intentionally carries hits, not the external-trigger records
needed for association. `vendor_passthrough` is a source-boundary policy rather
than a multi-stream event-building policy. Supporting either centrally requires
adding explicit source-event/trigger metadata to the input contract; it should
not be inferred from MIDAS delivery order.

Fitting and compression are independent switches. Supported compositions are:

| Fitting | Compression | Stored waveform payload |
|---|---|---|
| off | off | original `int16` samples |
| on | off | fit parameters plus exact `int32` residuals |
| off | on | encoded original sample integers |
| on | on | fit parameters plus encoded residuals |

The versioned output-bank header stores flags, fitter ID, codec ID, template
ID, sample count, fit parameters, payload byte/bit counts, channel identity,
and timestamp. Reconstruction therefore does not depend on the event builder's
current ODB settings.

The CPU template fitter uses a cubic-interpolated pulse template and scans a
configurable sub-sample time grid. At each time displacement it solves
amplitude and pedestal together as a two-parameter linear regression. Every
waveform product records chi-squared and degrees of freedom. Fits above
`Fitting/Maximum reduced chi squared` are flagged and retain their raw `int16`
samples; `Compression/Compress rejected fits` controls whether such fallbacks
may still be sent to a codec. `CubicPulseTemplate::from_aligned_waveforms()`
constructs a normalized template from a time-aligned calibration set. The
simulator benchmark uses the corresponding oversampled noiseless pulse truth.

The streaming simulator marks 75% of generated time groups as punch-through
positron candidates by producing low-amplitude hits across all eight ATAR
planes. `TopologyEventSelector` rejects an event only when the first and last
planes (and every intervening plane) are occupied and the mean integrated pulse
is below its configured threshold. Selection runs before fitting and
compression, making it the primary data-volume reduction stage.

## Build and test

```bash
./scripts/build.sh
./scripts/run_benchmark.sh --crates 2 --hits 100000 --fit 1 --compress 1
python3 scripts/run_rate_sweep.py --crates 1,2,4
python3 benchmarks/run_midas_stage_matrix.py --crates 2 --rate-hz 1000
```

The in-process report replaces `.artifacts/rate-tests/latest/report.md`. The
full MIDAS benchmark replaces the canonical
`.artifacts/midas-streaming-benchmark/{current,stage-matrix}` outputs.
The stage matrix includes `report.pdf`, with observed hits/s and MB/s for every
representation boundary plus the isolated service capacity of every stage.
For the full saturation and punch-through study run:

```bash
python3 benchmarks/run_midas_saturation.py \
  --rates 500,1000,2000,4000,8000,16000,32000,64000,128000
```

This replaces `.artifacts/midas-streaming-benchmark/saturation/report.pdf`.

Draft integration notes and historical design material are kept under the
ignored `.artifacts/` and `.context/` directories. They are not distributed as
project documentation.
