# SAMPIC 256-channel library 3.2

This directory contains the source and public headers copied from
`/home/pioneer/Pascal/Version_3.2_Linux` on 2026-09-30. Generated objects,
shared libraries, editor backups, examples, and machine-specific build files
were intentionally excluded.

The public header identifies the API as `SAMPIC_256CH_LIBRARY_VERSION "3.2"`.
The source package's Makefile instead contained `VERSION = 3.3.2`; this
repository uses the public API identifier and preserves this note to make the
upstream packaging inconsistency explicit.

The legacy `external/sampic_256ch_lib` submodule still provides version 3.1
and the lpdev/FTDI transport dependencies used by both versions.
