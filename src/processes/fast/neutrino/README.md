# sand::fast::neutrino

Fills the neutrino hypotheses of every reco interaction. Part of the [fast chain](../README.md).

| Port | Type |
|------|------|
| `in_truth` | `sand::caf::truth_branch_wrapper` |
| `in_common` | the output of [pid](../pid/README.md) |
| `out_common` | the same branch, with `nuhyp`, `Enu` and `dir` |

No parameters.

## What it does

Everything is copied from the true interaction (a declared cheat), except `part_energy_sum` and `part_mom_sum`,
which are sums over the visible **primary** reco particles (a primary's energy at generation already contains its
descendants').

## `SRNeutrinoHypothesisBranch` (`nuhyp.cvn`)

| Field | Filled | Value |
|-------|:------:|-------|
| `isnubar`, `nc`, `nue`, `numu`, `nutau` | ✅ cheat | 1 for the true class, else 0 |
| `protons0/1/2/N` | ✅ cheat | one-hot on the true number of protons (0, 1, 2, ≥3) |
| `chgpi0/1/2/N` | ✅ cheat | one-hot on the true π+ + π− |
| `pizero0/1/2/N` | ✅ cheat | one-hot on the true π0 |
| `neutron0/1/2/N` | ✅ cheat | one-hot on the true neutrons |

## `SRNeutrinoEnergyBranch` (`Enu`) [GeV]

| Field | Filled | Value |
|-------|:------:|-------|
| `calo`, `lep_calo`, `mu_range`, `mu_mcs`, `mu_mcs_llhd`, `e_calo`, `e_had`, `mu_had`, `regcnn` | ✅ cheat | true neutrino energy |
| `part_energy_sum` | ✅ | Σ `E` of the visible primaries |

## `SRDirectionBranch` (`dir`)

| Field | Filled | Value |
|-------|:------:|-------|
| `calo`, `heshw`, `lngtrk` | ✅ cheat | true neutrino direction |
| `part_mom_sum` | ✅ | direction of Σ `p` of the visible primaries |
