# sand::fast::ecal::reco

Builds one ECAL cluster per particle entering the ECAL, from the edep-sim hits. Part of the
[fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `out_ecal` | `sand::caf::ecal_reco_wrapper` (`SREcal`) |

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `min_energy_deposit` | 0 | MeV deposited in the cluster to keep it |

Instances: `sand::edep_reader`, `sand::spill_maps`.

## What it does

- A **cluster root** is a true particle born outside the ECAL, or a primary (an interaction in the ECAL).
- Its cluster collects the ECAL hits of the root and of all its descendants born inside the ECAL.
- A secondary born inside the ECAL is never a root: it belongs to the cluster of its ancestor.
- Any particle type can be a root, neutrons included.

## `SRECALCluster`

| Field | Filled | Value |
|-------|:------:|-------|
| `id` | ✅ | index in `ecal.clusters` |
| `E` | ✅ | visible energy, sum of the deposits [MeV], no sampling-fraction correction |
| `position` | ✅ | energy-weighted barycenter of the hit midpoints [cm] |
| `var_position` | ✅ | energy-weighted spread of the hits, with the legacy `neff` correction [cm]; narrower than the detector would see (hits are finer than cells) |
| `time` | ✅ | energy-weighted time [ns] |
| `start` | ✅ | start of the earliest hit [cm] |
| `direction` | ✅ | momentum direction of the root at its first ECAL point; (0, 0, 0) without one |
| `truth`, `truthOverlap` | ✅ | the root particle, 1 |
| `part` | ✅ | set later by [pid](../pid/README.md) |
| `num_cells` | ❌ | −1 (counting cells needs `sand::geoinfo`) |

`nclusters` is filled.

A particle crossing the ECAL twice (in and out of the barrel) gets a single cluster: its `position` is the
barycenter of both crossings and can lie between them.
