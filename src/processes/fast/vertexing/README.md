# sand::fast::vertexing

Creates the reco interactions from the assns: one per neutrino with at least one visible particle. Part of the
[fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `in_assns` | `sand::caf::sand_assn_wrapper` |
| `in_grain`, `in_tracker`, `in_ecal` | the three subdetector collections |
| `out_common` | `sand::caf::common_reco_branch_wrapper` |
| `out_nd` | `sand::caf::nd_reco_branch_wrapper` |

No parameters. Global instances: `sand::root_tgeomanager`, `sand::geoinfo`.

## What it does

- Copies the three subdetector collections into `nd.sand`.
- Every assn goes to the interaction of its true particle; the interaction is created at the first assn of that
  neutrino. `common.ixn.sandreco[i]` and `nd.sand.ixn[i]` are created together, so they stay index-aligned.
- The target is the material at the true vertex, from `sand::geoinfo`. The first call is slow (the geometry of the
  ECAL and of the tracker is built lazily, about a minute). A vertex outside GRAIN, the tracker and the ECAL gives
  `kOther` and one "invalid position" error line in the log.

## `SRInteraction` (`common.ixn.sandreco[i]`)

| Field | Filled | Value |
|-------|:------:|-------|
| `id` | ✅ | true interaction id |
| `vtx` | ✅ | true vertex [cm] |
| `time` | ✅ | true neutrino time [ns] |
| `truth`, `truthOverlap` | ✅ | `{index in truth.nu}`, `{1}` |
| `part` | ✅ | later, by [pid](../pid/README.md) |
| `dir`, `nuhyp`, `Enu` | ✅ | later, by [neutrino](../neutrino/README.md) |
| `preselected` | ❌ | `false` (default) |
| `isFromTrigger` | ❌ | `true` (default, correct: no cosmic overlay) |

`common.ixn.nsandreco` is filled.

## `SRSANDInt` (`nd.sand.ixn[i]`)

| Field | Filled | Value |
|-------|:------:|-------|
| `trkmatch`, `ntrkmatch` | ✅ | the assns of the interaction, in the order of the matching |
| `target` | ✅ | from the material at the true vertex, see below |

| Material in the geometry | `target` |
|--------------------------|----------|
| `Graphite` | `kGraphite` |
| `C3H6` | `kPlastic` |
| `LAr` | `kArgon` |
| `Lead`, `Scintillator` | `kCalo` |
| anything else (e.g. the STT frames), or outside the subdetectors | `kOther` |

The match is by material name: renaming a material in the GDML silently turns it into `kOther`.

`nd.sand.grain`, `tracker`, `ecal` (copies of the inputs) and `nd.sand.nixn` are filled.
