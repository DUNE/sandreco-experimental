# sand::fast::matching

Groups the objects of the three subdetectors by true particle: one `SRSANDAssn` per particle seen anywhere. Part of
the [fast chain](../README.md).

| Port | Type |
|------|------|
| `in_grain` | `sand::caf::grain_reco_wrapper` |
| `in_tracker` | `sand::caf::tracker_reco_wrapper` |
| `in_ecal` | `sand::caf::ecal_reco_wrapper` |
| `out_assns` | `sand::caf::sand_assn_wrapper` (vector of `SRSANDAssn`) |

No parameters.

## What it does

Perfect matching: two objects go together if their `truth[0]` is the same true particle. The assns are ordered by
interaction, then primaries before secondaries, then particle index.

## `SRSANDAssn`

| Field | Filled | Value |
|-------|:------:|-------|
| `constituents` | ✅ | `{collection, index}` of every object of the particle, at most one per subdetector, in scan order: GRAIN, tracker, ECAL |
| `trk.truth`, `trk.truthOverlap` | ✅ | the true particle, 1 |
| `part` | ✅ | set later by [pid](../pid/README.md) |
| `trk.start`, `end`, `dir`, `enddir`, `time`, `E`, `Evis`, `R`, `len_cm`, `len_gcm2`, `charge`, `qual` | ❌ | defaults (NaN): take them from the constituents |
| `trk.part` | ❌ | left at default by design (the back-link is `part`) |

The scan order is the crossing order for a particle going outwards, not for one going inwards.
