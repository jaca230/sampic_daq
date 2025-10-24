# Sampic Data Acquisition Midas Frontend

## External Dependencies

The hardware driver library `sampic_256ch_lib` is tracked as a git submodule in `external/sampic_256ch_lib`. After cloning this repository run:

```
git submodule update --init --recursive
```

The CMake build links directly against the shared objects provided by the submodule (including the FTDI and lpdev helper libraries). Rebuild the submodule with its Makefile only if you need a newer library version.

Running `scripts/build.sh` will automatically initialize submodules (if needed), configure CMake, and trigger the library build via the upstream Makefile so end users only need this single entry point. To rebuild the driver manually, invoke `make -C external/sampic_256ch_lib clean lib`.
