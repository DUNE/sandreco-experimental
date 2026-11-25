# 900_fast_reco test

`sand::test::fast_reco_test` checks the output of the [fast chain](../../../src/processes/fast/README.md)
(`truth_filler` → GRAIN, tracker and ECAL reco → `matching` → `vertexing` → `pid` → `neutrino`) field by field,
not just that it runs. Everything is recomputed from `sand::edep_reader`, the truth and the ROOT geometry, without
sharing code with the reco.

| Check | What must hold |
|-------|----------------|
| Counters | every `n*` field matches its vector; `common.ixn` and `nd.sand.ixn` are index-aligned |
| Interactions | one per neutrino with something visible, never empty, never two for the same neutrino; `id`, `vtx`, `time` are the true ones |
| Target | `SRSANDInt::target` is the material at the vertex (from the ROOT navigator, not `sand::geoinfo`) |
| Gate | a true particle has an object in a subdetector ig it left at least that subdetector's `min_energy_deposit` there (ECAL: iff it is a cluster root and its group passes); a particle has a reco particle iff it has an object |
| Links | particle `j` ↔ `trkmatch[j]` ↔ its objects (at most one per subdetector), same truth, every object used once |
| Tracks, showers | position, time, `len_cm`, `Evis`, `enddir` from the hits; `dir`, `E`, `R` at the entry point (NaN without one) |
| ECAL clusters | `E`, position, time, spread from the hits of the group; `direction` at the root's first ECAL point; `num_cells = -1` |
| Particles | `pdg`, `E` [GeV], `primary` from the truth; `parent` is the nearest visible ancestor, `daughters` mirror it |
| Neutrino | `part_energy_sum`, `part_mom_sum` are sums over the visible primaries |

The parameters must be the same as the recos: `tracker_min_energy_deposit`, `grain_min_energy_deposit`,
`ecal_min_energy_deposit` (MeV) and `b_field_magnitude` (T). Global instances: `sand::root_tgeomanager`.

Wired into `900_fast_reco.json`, `901_fast_reco_gauss_smear.json` and `902_fast_reco_gluck_smear.json`, after
`sand::fast::neutrino` and before the smearing. Any violation aborts via `UFW_ASSERT`, failing the ctest.

Run: `ctest -R '^90[0-2]_'` (after building and **installing** `sand_test_fast_reco`: `ufwrun` loads plugins
from `/usr/local/lib64`).
