# SAMPIC calibration data

Calibration data is crate-specific. Do not combine the contents of the two
crate directories: filenames overlap and identify different physical boards.

- `Crate_PIONEER_N1_LPNHE`: crate N1 at `192.168.0.13:27013`
- `Crate_PIONEER_N2_LPNHE`: crate N2 at `192.168.0.14:27014`

These datasets were copied on 2026-10-01 from Pascal's current calibration
directories under `/home/pioneer/Pascal/sampic`. Only calibration datasets
were imported: ADC ramp, ADC linearity, INL, internal-trigger threshold
offsets, and TOT calibration files. Software binaries and firmware bundles
from the source directory were intentionally excluded.

The frontend ODB should point at the appropriate crate directory, not at this
parent directory. Run the following command to configure both frontends:

```bash
./scripts/odb_tools/configure_dual_crates.py --apply
```
