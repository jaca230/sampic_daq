# Sampic Data Acquisition Midas Frontend

## External Dependencies

The hardware driver library `sampic_256ch_lib` is tracked as a git submodule in `external/sampic_256ch_lib`. After cloning this repository run:

```
git submodule update --init --recursive
```

The CMake build links directly against the shared objects provided by the submodule (including the FTDI and lpdev helper libraries). Rebuild the submodule with its Makefile only if you need a newer library version.

Running `scripts/build.sh` will automatically initialize submodules (if needed), configure CMake, and trigger the library build via the upstream Makefile so end users only need this single entry point. To rebuild the driver manually, invoke `make -C external/sampic_256ch_lib clean lib`.

## Development environment

The project uses a local micromamba environment containing ROOT, Python,
CMake, Ninja, Make, and pkg-config. The environment and package cache remain
inside the ignored `.venv` directory; no system Conda installation is needed.

Activate it in each new shell:

```bash
source scripts/setup_env.sh
```

On first use, the activation script automatically downloads micromamba and
creates the environment. It also configures MIDAS, its Python package, and
the in-tree SAMPIC runtime libraries. Paths are derived from the repository
layout when the MIDAS variables are not already set. For a different machine
layout, copy `.env.example` to the gitignored `.env` and set `MIDASSYS`,
`MIDAS_EXPT_NAME`, and `MIDAS_EXPTAB` directly.

To explicitly update, recreate, or inspect the environment operation, use
`scripts/environment/create_env.sh`.

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
are `default` and `external_trigger`; SAMPIC collector, controller init, and
controller apply modes each provide `default`, `example`, and `simulator`.

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
```

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
