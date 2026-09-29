# SAMPIC test data

Generated data under this directory is intentionally excluded from Git.

The large scan directories are stored on the `/data01` network filesystem at:

```text
/data01/pioneer/midas_sampic/sampic_daq/sampic_tests_data/
```

The local entries `external_trigger_batching_scan` and
`external_trigger_probe` are symbolic links to their corresponding directories
there. Keeping these links means existing acquisition and analysis paths still
work while new data is written to `/data01` rather than the system disk.

The files already tracked in Git in this directory predate this policy. New
generated datasets and scan outputs must not be added to Git.
