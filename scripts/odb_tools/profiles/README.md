# SAMPIC ODB profiles

Profiles are complete, named configurations layered on the ODB tree generated
by the frontend. Dry-run is always the default.

```bash
./scripts/odb_tools/profiles/apply_profile.py list
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger --apply
./scripts/odb_tools/profiles/apply_profile.py self_trigger --channels 0:3,0:14,0:15 --apply
./scripts/odb_tools/profiles/apply_profile.py vendor_passthrough --apply
./scripts/odb_tools/profiles/apply_profile.py time_grouped --apply
```

The operational profiles are:

- `l2_external_trigger`: apply the tested external-gated acquisition settings
  and select trigger-record association (`external_gated_trigger`).
- `self_trigger`: configure a clean per-channel self-trigger setup, disable all
  channels not explicitly listed, and remove stale external-gate/L2 settings.
- `vendor_passthrough`: preserve one decoded vendor event per MIDAS event and
  disable all hit-time grouping.
- `time_grouped`: select the standard timestamp-clustering mode and configure
  its grouping/finalization windows, vendor frame batching, and collector
processing interval.

Hardware-trigger and collector profiles are intentionally composable. For the
known N1 channel set with a 10 us grouping window, use:

```bash
./scripts/odb_tools/profiles/apply_profile.py self_trigger \
  --frontend-index 0 \
  --channels 0:3,0:14,0:15,3:48,3:49,3:51,3:60,3:62,3:63 \
  --threshold-volts 0.15 \
  --apply

./scripts/odb_tools/profiles/apply_profile.py time_grouped \
  --frontend-index 0 \
  --time-window-ns 10000 \
  --apply
```

For an occupancy/debug run in which every channel should discriminate at the
same threshold, use the explicit `all` selection:

```bash
./scripts/odb_tools/profiles/apply_profile.py self_trigger \
  --frontend-index 0 \
  --channels all \
  --threshold-volts 0.15 \
  --apply
```

Profiles affect the next run configuration refresh. The frontend executable
must already include the selected mode so its typed ODB subtree has been
initialized; restart a newly rebuilt frontend once before applying a new mode
for the first time.

Hardware selection and profile-specific overrides are shown by:

```bash
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger --help
```

To add a profile, create one module in this directory containing an
`OdbProfile` subclass and export one instance as `PROFILE`. The runner
discovers it automatically; no central import list needs updating.

`reset_sampic_odb.py` is kept here because it is configuration maintenance,
but it is deliberately separate from profiles: resetting snapshots and
deletes the entire equipment subtree instead of assigning profile values.

## Dual-crate connections

Configure the PIONEER crate mapping after the frontend has created both ODB
trees. Dry-run is the default:

```bash
./scripts/odb_tools/configure_dual_crates.py
./scripts/odb_tools/configure_dual_crates.py --apply
```

This maps frontend 00 to N1 (`192.168.0.13:27013`) and frontend 01 to N2
(`192.168.0.14:27014`) and assigns each crate's calibration directory.

## Channel sets

Use FEB-wide channel numbers, matching the occupancy report. This example
disables all other channels on frontend 01, enables N2 FEB 0 channels 25–31,
includes them in central triggering, and assigns a 0.15 V threshold:

```bash
./scripts/odb_tools/set_channel_set.py \
  --crate n2 \
  --channels 0:25,0:26,0:27,0:28,0:29,0:30,0:31 \
  --threshold 0.15 \
  --disable-others

# Repeat with --apply after reviewing the dry-run.
```

`bulk_set_channels.py` remains useful for rectangular board/SAMPIC/local-channel
selections and arbitrary channel fields. `set_channel_set.py` is intended for
explicit physical channel lists discovered by occupancy scans.
