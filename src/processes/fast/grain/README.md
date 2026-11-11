# sand::fast::grain::reco

The same algorithm as [`sand::fast::tracker::reco`](../tracker/README.md), on the GRAIN liquid argon volume
instead of the tracker. Part of the [fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `out_grain` | `sand::caf::grain_reco_wrapper` (`SRGRAIN`) |

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `min_energy_deposit` | 0 | MeV deposited in GRAIN to see a particle |
| `b_field_magnitude` | 0.6 | T, field along x, for `R` |

Instances: `sand::edep_reader`, `sand::spill_maps`.

## Fields

`SRTrack` and `SRShower` are filled exactly as in the [tracker tables](../tracker/README.md#srtrack), with "GRAIN"
in place of "tracker": hits, deposits and the entry point are those in GRAIN. For a particle born in GRAIN the entry
point is where it is created.

Optimistic on purpose: GRAIN measures no curvature, but `R` and `E` are filled with the tracker formula.
