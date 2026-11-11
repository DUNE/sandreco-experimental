# sand::fast::tracker::reco

Builds the tracker objects (STT or drift chambers) from the edep-sim hits of every true particle. Part of the
[fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `out_tracker` | `sand::caf::tracker_reco_wrapper` (`SRTracker`) |

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `min_energy_deposit` | 0 | MeV deposited in the tracker to see a particle |
| `b_field_magnitude` | 0.6 | T, field along x, for `R` |

Instances: `sand::edep_reader`, `sand::spill_maps`.

## What it does

For every true particle (primaries, then secondaries) with hits in the tracker summing to at least
`min_energy_deposit`:

- μ±, π±, K±, p, p̄ → one `SRTrack`;
- e±, γ, π0 → one `SRShower` (in practice e± only: neutrals own no hits);
- anything else (neutrons, nuclei) → nothing.

"Entry point" below is the earliest edep-sim trajectory point in the tracker.

## `SRTrack`

| Field | Filled | Value |
|-------|:------:|-------|
| `start` | ✅ | start of the earliest hit [cm] |
| `end` | ✅ | stop of the latest hit [cm] |
| `dir` | ✅ | momentum direction at the entry point; direction of the first hit without one |
| `enddir` | ✅ | direction of the last hit |
| `time` | ✅ | start time of the earliest hit [ns] |
| `Evis` | ✅ | energy deposited in the tracker [MeV] |
| `len_cm` | ✅ | sum of the hit lengths [cm] (not the start–end distance) |
| `E` | ✅ | √(p² + m²) at the entry point, mass from the true pdg [MeV]; NaN without an entry point |
| `R` | ✅ | √(p_y² + p_z²) / (0.3 B) at the entry point [cm]; NaN without an entry point |
| `charge` | ✅ | from the true pdg |
| `qual` | ✅ | 1 |
| `truth`, `truthOverlap` | ✅ | the true particle, 1 |
| `part` | ✅ | set later by [pid](../pid/README.md) |
| `len_gcm2` | ❌ | NaN (needs the materials along the track) |

## `SRShower`

| Field | Filled | Value |
|-------|:------:|-------|
| `start` | ✅ | start of the earliest hit [cm] |
| `direction` | ✅ | momentum direction at the entry point; direction of the first hit without one |
| `time` | ✅ | start time of the earliest hit [ns] |
| `Evis` | ✅ | energy deposited in the tracker [MeV] |
| `truth`, `truthOverlap` | ✅ | the true particle, 1 |
| `part` | ✅ | set later by [pid](../pid/README.md) |
| `qual` | ❌ | NaN |
| `len_cm` | ❌ | NaN |
| `initial_dEdx`, `conversionGap` | ❌ | 0 (need a shower-shape reconstruction) |

`ntracks` and `nshowers` are filled.
