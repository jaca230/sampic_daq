# Sampic Data Acquisition Midas Frontend

## External Dependencies

The hardware driver library `sampic_256ch_lib` is tracked as a git submodule in `external/sampic_256ch_lib`. After cloning this repository run:

```
git submodule update --init --recursive
```

The CMake build links directly against the shared objects provided by the submodule (including the FTDI and lpdev helper libraries). Rebuild the submodule with its Makefile only if you need a newer library version.

Running `scripts/build.sh` will automatically initialize submodules (if needed), configure CMake, and trigger the library build via the upstream Makefile so end users only need this single entry point. To rebuild the driver manually, invoke `make -C external/sampic_256ch_lib clean lib`.
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
