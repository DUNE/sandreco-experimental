# Fields filled by the `fake_reco` process

This document describes which CAF structure fields are filled by `fake_reco` using the `CAFFiller<T>` templates.

---

## 1. `caf::SRTrueInteraction` (Truth)

Stored in `m_caf->mc.nu`. Filled by `CAFFiller<SRTrueInteraction>::from_genie()`.

| Field | Type | Source | Notes |
|-------|------|--------|-------|
| **Identifiers** ||||
| `id` | `long int` | `GRooTrackerEvent::EvtNum_` | Event number |
| `genieIdx` | `long int` | `GRooTrackerEvent::EvtNum_` | Same as id |
| **Neutrino** ||||
| `pdg` | `int` | `EventSummary::probe_pdg` | From `nu:` token |
| `pdgorig` | `int` | = `pdg` | No oscillation for ND |
| `iscc` | `bool` | `interaction_type == "Weak[CC]"` | |
| `mode` | `ScatteringMode` | `EventSummary::scattering_type` | QE, RES, DIS, MEC... |
| **Target** ||||
| `targetPDG` | `int` | `EventSummary::target_pdg` | From `tgt:` token |
| `hitnuc` | `int` | `EventSummary::hit_nucleon_pdg` | 0 if N/A |
| **Energy/Kinematics** ||||
| `E` | `float` | `StdHep::P4_[0].E()` | Nu energy [GeV] |
| `momentum` | `SRVector3D` | `StdHep::P4_[0].Vect()` | Nu 3-momentum |
| `vtx` | `SRVector3D` | `GRooTrackerEvent::EvtVtx_[0-2]` | Vertex [cm] |
| `time` | `float` | `GRooTrackerEvent::EvtVtx_[3]` | Vertex time |
| `isvtxcont` | `bool` | Hardcoded `true` | |
| **Calculated kinematics** ||||
| `Q2` | `float` | `Kinematics::calculate()` | -q² |
| `q0` | `float` | `Kinematics::calculate()` | Energy transfer |
| `modq` | `float` | `Kinematics::calculate()` | \|**q**\| |
| `W` | `float` | `Kinematics::calculate()` | Hadronic mass |
| `bjorkenX` | `float` | `Kinematics::calculate()` | Q²/(2Mq₀) |
| `inelasticity` | `float` | `Kinematics::calculate()` | y = q₀/Eν |
| **GENIE info** ||||
| `ischarm` | `bool` | `EventSummary::is_charm_event` | |
| `isseaquark` | `bool` | DIS + `hit_sea_quark` | |
| `resnum` | `int` | `EventSummary::resonance_type` | Only for RES |
| `xsec` | `float` | `GRooTrackerEvent::EvtXSec_` | |
| `genweight` | `float` | `GRooTrackerEvent::EvtWght_` | |
| `generator` | `Generator` | Hardcoded `kGENIE` | |
| `xsec_cvwgt` | `float` | Hardcoded `1.0f` | |
| **Particle counters** (via `increment_particle_counter`) ||||
| `nproton` | `int` | PDG 2212 count | From prim + sec |
| `nneutron` | `int` | PDG 2112 count | From prim + sec |
| `npip` | `int` | PDG 211 count | π⁺ |
| `npim` | `int` | PDG -211 count | π⁻ |
| `npi0` | `int` | PDG 111 count | π⁰ |
| **Particle vectors** ||||
| `nprim` | `int` | `add_primaries()` | Count |
| `prim` | `vector<SRTrueParticle>` | `add_primaries()` | Post-FSI |
| `nsec` | `int` | `add_secondaries()` | Count |
| `sec` | `vector<SRTrueParticle>` | `add_secondaries()` | GEANT secondaries |
| `nprefsi` | `int` | `add_prefsi()` | Count |
| `prefsi` | `vector<SRTrueParticle>` | `add_prefsi()` | Pre-FSI hadrons |

**Filled:** 35 fields

---

## 2. `caf::SRTrueParticle` (Truth Particles)

Stored in `SRTrueInteraction.prim`, `.sec`, `.prefsi`.

### From edep-sim (`from_edep`)

| Field | Type | Source |
|-------|------|--------|
| `pdg` | `int` | `EDEPTrajectory::GetPDGCode()` |
| `G4ID` | `int` | `EDEPTrajectory::GetId()` |
| `interaction_id` | `long int` | Parameter |
| `ancestor_id` | `TrueParticleID` | Parameter |
| `parent` | `int` | `EDEPTrajectory::GetParentId()` |
| `p` | `SRLorentzVector` | `GetInitialMomentum()` |
| `time` | `float` | First trajectory point |
| `start_pos` | `SRVector3D` | First trajectory point |
| `end_pos` | `SRVector3D` | Last trajectory point |
| `first_process` | `unsigned int` | First point process |
| `first_subprocess` | `unsigned int` | First point subprocess |
| `end_process` | `unsigned int` | Last point process |
| `end_subprocess` | `unsigned int` | Last point subprocess |

### From GENIE (`from_genie`) - for prefsi only

| Field | Type | Source |
|-------|------|--------|
| `pdg` | `int` | `StdHep::Pdg_[index]` |
| `G4ID` | `int` | Hardcoded `-1` |
| `interaction_id` | `long int` | Parameter |
| `p` | `SRLorentzVector` | `StdHep::P4_[index]` |

**Filled:** 13 fields (edep), 4 fields (genie)

---

## 3. `caf::SRInteraction` (Reco - Common Branch)

Stored in `m_caf->common.ixn.sandreco`. Filled by `CAFFiller<SRInteraction>::from_true()` + main loop.

| Field | Type | Source |
|-------|------|--------|
| `id` | `long int` | `SRTrueInteraction.id` |
| `vtx` | `SRVector3D` | `SRTrueInteraction.vtx` |
| `Enu.calo` | `float` | `SRTrueInteraction.E` |
| `truth` | `vector<size_t>` | `{truth_index}` |
| `truthOverlap` | `vector<float>` | `{1.0f}` |
| `dir.part_mom_sum` | `SRVector3D` | Sum of particle momenta (main loop) |
| `part.nsandreco` | `int` | Particle count (main loop) |
| `part.sandreco` | `vector<SRRecoParticle>` | (main loop) |

**Filled:** 8 fields

---

## 4. `caf::SRRecoParticle` (Reco Particles)

Stored in `SRInteraction.part.sandreco`. Filled by `CAFFiller<SRRecoParticle>::from_true()`.

| Field | Type | Source |
|-------|------|--------|
| `primary` | `bool` | Hardcoded `true` |
| `pdg` | `int` | `SRTrueParticle.pdg` |
| `score` | `float` | Hardcoded `1.0f` |
| `E` | `float` | `SRTrueParticle.p.E` |
| `E_method` | `PartEMethod` | Hardcoded `kCalorimetry` |
| `p` | `SRVector3D` | `(p.px, p.py, p.pz)` |
| `start` | `SRVector3D` | `SRTrueParticle.start_pos` |
| `end` | `SRVector3D` | `SRTrueParticle.end_pos` |
| `origRecoObjType` | `RecoObjType` | `is_track_like()` → kTrack, `is_shower_like()` → kShower |
| `truth` | `vector<TrueParticleID>` | `{id}` |
| `truthOverlap` | `vector<float>` | `{1.0f}` |

**Filled:** 11 fields

---

## 5. `caf::SRSANDInt` (SAND-specific Reco)

Stored in `m_caf->nd.sand.ixn`. Filled in main loop.

| Field | Type | Source |
|-------|------|--------|
| `ntracks` | `size_t` | Track count |
| `tracks` | `vector<SRTrack>` | `CAFFiller<SRTrack>::from_true()` |
| `nshowers` | `size_t` | Shower count |
| `showers` | `vector<SRShower>` | `CAFFiller<SRShower>::from_true()` |

**Filled:** 4 fields

---

## 6. `caf::SRTrack` (SAND Tracks)

Stored in `SRSANDInt.tracks`. Filled by `CAFFiller<SRTrack>::from_true()`.

| Field | Type | Source |
|-------|------|--------|
| `start` | `SRVector3D` | `SRTrueParticle.start_pos` |
| `end` | `SRVector3D` | `SRTrueParticle.end_pos` |
| `dir` | `SRVector3D` | `normalize_to_direction(px, py, pz)` |
| `enddir` | `SRVector3D` | = `dir` |
| `time` | `double` | `SRTrueParticle.time` |
| `E` | `float` | `p.E * 1000` [MeV] |
| `Evis` | `float` | = `E` |
| `len_cm` | `float` | `distance(start, end)` |
| `charge` | `short int` | `charge_from_pdg(pdg)` |
| `qual` | `float` | Hardcoded `1.0f` |
| `truth` | `vector<TrueParticleID>` | `{id}` |
| `truthOverlap` | `vector<float>` | `{1.0f}` |

**Filled:** 12 fields

---

## 7. `caf::SRShower` (SAND Showers)

Stored in `SRSANDInt.showers`. Filled by `CAFFiller<SRShower>::from_true()`.

| Field | Type | Source |
|-------|------|--------|
| `start` | `SRVector3D` | `SRTrueParticle.start_pos` |
| `direction` | `SRVector3D` | `normalize_to_direction(px, py, pz)` |
| `Evis` | `float` | `p.E * 1000` [MeV] |
| `truth` | `vector<TrueParticleID>` | `{id}` |
| `truthOverlap` | `vector<float>` | `{1.0f}` |

**Filled:** 5 fields (100%)

---

## Spill-level Counters

Set in `initialize_spill_capacities()` and main loop:

| Field | Location | Source |
|-------|----------|--------|
| `mc.nnu` | Truth | Number of interactions |
| `common.ixn.nsandreco` | Reco | Incremented per interaction |
| `nd.sand.nixn` | SAND | Incremented per interaction |

---

## PDG Classification

Defined in `caf_filler_common.hpp`.

### Track-like (`is_track_like`)
| PDG | Particle |
|-----|----------|
| ±13 | μ± |
| ±211 | π± |
| ±321 | K± |
| 2212 | p |

### Shower-like (`is_shower_like`)
| PDG | Particle |
|-----|----------|
| ±11 | e± |
| 22 | γ |
| 111 | π⁰ |

---

## Fields NOT Filled

### SRTrueInteraction
- `removalE`, `t` - Not available from gRooTracker
- `baseline`, `prod_vtx`, `parent_dcy_*`, `imp_weight` - Require flux info
- `genVersion` - Not exposed

### SRTrueParticle
- `daughters` - Available via EDEPTree, not stored

### SRInteraction
- `dir.lngtrk`, `dir.heshw`, `dir.calo` - Require real reco
- `nuhyp.cvn.*` - Require CVN
- `Enu.*` (except calo) - Require reco algorithms
- `preselected` - Selection flag

### SRRecoParticle
- `contained`, `walldist` - Require geometry
- `tgtA`, `parent`, `daughters` - Not applicable

### SRSANDInt
- `nclusters`, `ECALClusters` - Require ECAL reco

### SRTrack
- `len_gcm2` - Require material info

---

## Related Files

| File | Content |
|------|---------|
| `fake_reco.cpp` | Main loop, spill init, particle processing |
| `fake_reco.hpp` | Class declaration |
| `caf_filler.hpp` | CAFFiller template declarations |
| `caf_filler_common.hpp` | PDG utils, vector math, truth matching |
| `caf_filler_true.cpp` | `CAFFiller<SRTrueParticle>`, `CAFFiller<SRTrueInteraction>` |
| `caf_filler_reco.cpp` | `CAFFiller<SRRecoParticle>`, `CAFFiller<SRTrack>`, `CAFFiller<SRShower>`, `CAFFiller<SRInteraction>` |
| `genie_helpers/EvtCode_parser.hpp` | `EventSummary` parser |
