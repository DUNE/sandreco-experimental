# sand::fast::pid

Builds one reco particle per assn and links particles, assns and subdetector objects. Part of the
[fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `in_common`, `in_nd` | the output of [vertexing](../vertexing/README.md) |
| `out_common`, `out_nd` | the same branches, with the particles and the links |

No parameters.

## What it does

- Particle `j` of interaction `i` is built from `nd.sand.ixn[i].trkmatch[j]`.
- Perfect PID: the kinematics are those of the true particle at generation.
- `parent` is the nearest **visible** true ancestor, so a secondary whose parent was not seen gets `parent = -1`
  while keeping `primary = false`.

## `SRRecoParticle` (`common.ixn.sandreco[i].part.sandreco[j]`)

| Field | Filled | Value |
|-------|:------:|-------|
| `primary` | ✅ | true particle is a primary |
| `pdg` | ✅ | true pdg |
| `score` | ✅ | 1 |
| `E` | ✅ | true energy at generation [GeV] |
| `p` | ✅ | true momentum at generation [GeV/c] |
| `start`, `end` | ✅ | true start and end positions [cm] (not where the objects start) |
| `origRecoObjType` | ✅ | from the first constituent: track (GRAIN, tracker) → `kTrack`; shower or ECAL cluster → `kShower` |
| `parent`, `daughters` | ✅ | nearest visible ancestor, and its mirror |
| `truth`, `truthOverlap` | ✅ | the true particle, 1 |
| `recoobj` | ✅ | `{i, kSANDAssn, j}` → `nd.sand.ixn[i].trkmatch[j]` |
| `tgtA` | ❌ | 0 |
| `E_method` | ❌ | `kUnknownMethod` |
| `contained` | ❌ | `false` |
| `walldist` | ❌ | NaN |

`part.nsandreco` is filled.

Particles seen only in the ECAL (the majority, neutrons included) get `origRecoObjType = kShower`.

## Links

| From | Field | To |
|------|-------|----|
| particle | `recoobj` | `nd.sand.ixn[i].trkmatch[j]` |
| assn | `part` | `{i, kSandreco, j}` → the particle |
| every constituent (track, shower, cluster) | `part` | `{i, kSandreco, j}` → the particle |
| any of them | `truth[0]` | `truth.nu[ixn].prim` or `.sec` |
