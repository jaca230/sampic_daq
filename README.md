# Sampic Data Acquisition Midas Frontend

## External Dependencies

SAMPIC library 3.2 is vendored as source in
`external/sampic_256ch_lib_3_2` and is the default. The original 3.1 library
remains available in the `external/sampic_256ch_lib` git submodule, which also
provides the lpdev and FTDI transport dependencies used by both versions.
After cloning this repository run:

```
git submodule update --init --recursive
```

The CMake build compiles the selected SAMPIC source package and links it with
the transport libraries supplied by the submodule. Version 3.2 is selected by
default. To build against the legacy implementation instead, run:

```bash
./scripts/build.sh --sampic-version 3.1
```

Running `scripts/build.sh` initializes submodules if needed, configures CMake,
and builds the selected SAMPIC library and frontend. Direct CMake builds can
select the implementation with `-DSAMPIC_LIBRARY_VERSION=3.2` or `3.1`.

## Runtime and development environments

For normal DAQ operation, activate the minimal runtime environment:

```bash
source scripts/setup_env.sh
```

This only configures paths for the built frontend, MIDAS, and the SAMPIC
runtime libraries. It never downloads or installs software.

For development, activate the project-local micromamba environment containing
ROOT, Python, CMake, Ninja, Make, pkg-config, and analysis tools:

```bash
source scripts/setup_env.sh --dev
```

On first use, `--dev` automatically downloads a repository-local micromamba
binary and creates the environment from
`scripts/environment/environment.yml`. The environment and package cache stay
inside the ignored `.venv` directory; no system Conda installation or shell
initialization is required. Subsequent activation does not update packages
automatically.

Both modes configure MIDAS and the in-tree SAMPIC runtime libraries. Paths are
derived from the repository layout when the MIDAS variables are not already
set. For a different machine layout, copy `.env.example` to the gitignored
`.env` and set `MIDASSYS`, `MIDAS_EXPT_NAME`, and `MIDAS_EXPTAB` directly.

To explicitly install, update, recreate, or inspect the developer environment,
use `scripts/environment/create_env.sh`.

Verify the active environment with:

```bash
./scripts/environment/check_env.sh
```

The previous `scripts/environment_setup` directory and the external
`~/jcarlton/software/sampic_dev` Python-only venv are obsolete.

## Registry-driven ODB configuration

The frontend owns a registry-driven tree below
`/Equipment/SAMPIC XX/Settings`:

```text
Logger/
Frontend/
Crate/
  <crate setting>
  front_end_boards/febN/
    <FEB setting>
    sampics/sampicN/
      <SAMPIC setting>
      channels/channelN/<channel setting>
Sampic Controller/
  init_mode
  apply_mode
  init_modes/<mode-id>/<typed settings>
  apply_modes/<mode-id>/<typed settings>
Sampic Event Collector/
  mode
  buffer_size
  sleep_time_us
  modes/<mode-id>/<typed settings>
Frontend Event Collector/
  mode
  buffer_size
  sleep_time_us
  diagnostics/<setting>
  modes/<mode-id>/<typed settings>
```

Mode selectors use canonical lower-case IDs. Available frontend collector modes
are:

- `default`: cluster hits from one or more decoded SAMPIC events by timestamp.
- `external_gated_trigger`: associate hits with decoded external-trigger
  records and emit one MIDAS event per trigger. The older `external_trigger`
  ID remains as a compatible alias.
- `vendor_passthrough`: emit exactly one MIDAS event per decoded vendor
  `EventStruct`, without time grouping or trigger-based splitting. Its `AD`
  bank contains all corrected hits from that vendor event and its `VT` bank
  retains the vendor trigger records.

The packed `VTxx` payload starts with `uint32_t trigger_count`, followed by
that many records containing `uint32_t fpga_trigger_id`, `uint32_t
external_trigger_id`, `uint16_t spill_number`, `uint16_t raw_extra_word`, and
`double trigger_timestamp_ns`, in that order.

SAMPIC collector, controller init, and controller apply modes each provide
`default`, `example`, and `simulator`.

Hardware settings are validated in full before any vendor setter runs. They
are then checked deterministically in crate → FEB → SAMPIC → channel order,
followed by each descriptor's explicit priority and setting ID. A fast vendor
getter is used first and the setter is called only when the requested value
differs (with tolerant floating-point comparison).

Each concrete mode owns a directory containing its configuration and
implementation, such as `modes/default/default_config.h` and
`modes/default/default_mode.{h,cpp}`. Mode translation units self-register;
collectors and the controller do not include a list of concrete modes.

Hardware descriptors are split by crate/FEB/SAMPIC/channel under
`src/integration/sampic/settings/descriptors`. Descriptor translation units
also self-register, so a new descriptor provider can be added without editing
the controller or a central registry manifest.

### ODB profiles and maintenance tools

Named operational configurations live in `scripts/odb_tools/profiles`.
Profiles are dry-run by default:

```bash
./scripts/odb_tools/profiles/apply_profile.py list
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger --apply
./scripts/odb_tools/profiles/apply_profile.py vendor_passthrough --apply
./scripts/odb_tools/profiles/apply_profile.py time_grouped --apply
```

The `l2_external_trigger` profile selects `external_gated_trigger` and applies
the full tested acquisition setup: self-trigger primitives, FEB L2 OR logic,
external-gate coincidence, trigger counters, 31 frames per block, 127 triggers
per vendor event, zero added collector sleeps, and enlarged buffers. Override
the hardware selection, timing windows, sampling frequency, or packetization
with the profile's command-line options.

`vendor_passthrough` switches only collection/event-building behavior.
`time_grouped` also configures vendor frame batching and the grouping worker's
processing interval, but it does not rewrite channel trigger settings.

Each profile owns its arguments and documented ODB writes in one Python
module. The runner discovers profile modules automatically, so adding a
profile does not require changing a central import list.

Resetting the generated equipment tree is destructive and remains a separate
maintenance command:

```bash
./scripts/odb_tools/profiles/reset_sampic_odb.py
./scripts/odb_tools/profiles/reset_sampic_odb.py --apply
```

The board, SAMPIC, and channel bulk setters remain universal tools directly
under `scripts/odb_tools`.

For the two PIONEER crates, initialize the persistent frontend mapping with:

```bash
./scripts/odb_tools/configure_dual_crates.py          # dry-run
./scripts/odb_tools/configure_dual_crates.py --apply
```

This maps frontend 00 to N1 (`192.168.0.13:27013`) and frontend 01 to N2
(`192.168.0.14:27014`), including their calibration directories. Explicit
channel lists can then be configured using FEB-wide channel numbers from an
occupancy scan:

```bash
./scripts/odb_tools/set_channel_set.py \
  --crate n2 \
  --channels 0:25,0:26,0:27,0:28,0:29,0:30,0:31 \
  --threshold 0.15 \
  --disable-others
```

Both commands are dry-run by default; add `--apply` after reviewing them.
