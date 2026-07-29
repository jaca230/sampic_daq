# SAMPIC ODB profiles

Profiles are complete, named configurations layered on the ODB tree generated
by the frontend. Dry-run is always the default.

```bash
./scripts/odb_tools/profiles/apply_profile.py list
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger
./scripts/odb_tools/profiles/apply_profile.py l2_external_trigger --apply
```

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
