# Fields filled by the `fake_reco` process

This document describes which CAF structure fields (defined in `duneanaobj`) are filled by the `fake_reco` process.

---

## 1. `caf::SRTrueInteraction` (Truth)

Structure for MC interaction information. Stored in `standard_record_->mc.nu`.

| Field | Type | Filled | Source | Notes |
|-------|------|--------|--------|-------|
| **Identifiers** |||||
| `id` | `long int` | Yes | GENIE `EvtNum_` | `fake_reco.cpp:44` |
| `genieIdx` | `long int` | Yes | GENIE `EvtNum_` | `fake_reco.cpp:45` |
| **Neutrino** |||||
| `pdg` | `int` | Yes | GENIE EvtCode parser | Neutrino PDG |
| `pdgorig` | `int` | Yes | = `pdg` | No oscillation for ND |
| `iscc` | `bool` | Yes | GENIE EvtCode | `Weak[CC]` |
| `mode` | `ScatteringMode` | Yes | GENIE EvtCode | kQE, kRes, kDIS, kCoh, kMEC... |
| **Target** |||||
| `targetPDG` | `int` | Yes | GENIE EvtCode | Nuclear target PDG |
| `hitnuc` | `int` | Yes | GENIE EvtCode | Hit nucleon (0 if N/A) |
| `removalE` | `float` | No | - | Not implemented |
| **Energy/Kinematics** |||||
| `E` | `float` | Yes | GENIE StdHep | Nu energy [GeV] |
| `momentum` | `SRVector3D` | Yes | GENIE StdHep | Nu 3-momentum |
| `vtx` | `SRVector3D` | Yes | GENIE `EvtVtx_` | Vertex [cm] |
| `isvtxcont` | `bool` | Yes | Hardcoded `true` | No cosmic/rock nu simulated |
| `time` | `float` | Yes | GENIE `EvtVtx_[3]` | Vertex time |
| **Kinematic variables** |||||
| `bjorkenX` | `float` | Yes | Calculated | Q^2/(2*M*q0) |
| `inelasticity` | `float` | Yes | Calculated | y = q0/Enu |
| `Q2` | `float` | Yes | Calculated | -q^2 |
| `q0` | `float` | Yes | Calculated | Energy transfer |
| `modq` | `float` | Yes | Calculated | |q| |
| `W` | `float` | Yes | Calculated | Hadronic invariant mass |
| `t` | `float` | No | - | Only for Coh/Diffractive, not impl. |
| **GENIE info** |||||
| `ischarm` | `bool` | Yes | GENIE EvtCode | Charm event |
| `isseaquark` | `bool` | Yes | GENIE EvtCode | Sea quark (DIS only) |
| `resnum` | `int` | Yes | GENIE EvtCode | Only for mode=kRes |
| `xsec` | `float` | Yes | GENIE `EvtXSec_` | Cross section [1/GeV^2] |
| `genweight` | `float` | Yes | GENIE `EvtWght_` | Generator weight |
| **Nu production (flux)** |||||
| `baseline` | `float` | No | - | From NuParent, not available |
| `prod_vtx` | `SRVector3D` | No | - | From NuParent, not available |
| `parent_dcy_mom` | `SRVector3D` | No | - | From NuParent, not available |
| `parent_dcy_mode` | `int` | No | - | From NuParent, not available |
| `parent_pdg` | `int` | No | - | From NuParent, not available |
| `parent_dcy_E` | `float` | No | - | From NuParent, not available |
| `imp_weight` | `float` | No | - | From NuParent, not available |
| **Generator** |||||
| `generator` | `Generator` | Yes | Hardcoded `kGENIE` | |
| `genVersion` | `vector<uint>` | No | - | Not implemented |
| **Particle counters** |||||
| `nproton` | `int` | Yes | Loop over prim+sec | Counted from edep-sim |
| `nneutron` | `int` | Yes | Loop over prim+sec | Counted from edep-sim |
| `npip` | `int` | Yes | Loop over prim+sec | pi+ |
| `npim` | `int` | Yes | Loop over prim+sec | pi- |
| `npi0` | `int` | Yes | Loop over prim+sec | pi0 |
| **Particle vectors** |||||
| `nprim` | `int` | Yes | edep-sim | Direct vertex children |
| `prim` | `vector<SRTrueParticle>` | Yes | edep-sim | Post-FSI primary particles |
| `nprefsi` | `int` | Yes | GENIE StdHep | Pre-FSI hadrons |
| `prefsi` | `vector<SRTrueParticle>` | Yes | GENIE StdHep | `kIStHadronInTheNucleus` |
| `nsec` | `int` | Yes | edep-sim | Secondaries from GEANT |
| `sec` | `vector<SRTrueParticle>` | Yes | edep-sim | Secondary particles |
| **Weights** |||||
| `xsec_cvwgt` | `float` | Yes | Hardcoded `1` | Central value weight |

**Truth Summary:** 35/43 fields filled

---

## 2. `caf::SRInteraction` (Reco - Common Branch)

Structure for reconstructed interaction. Stored in `standard_record_->common.ixn.sandreco`.

| Field | Type | Filled | Value | Notes |
|-------|------|--------|-------|-------|
| `id` | `long int` | Yes | `SRTrueInteraction.id` | Same ID as truth |
| `vtx` | `SRVector3D` | Yes | `SRTrueInteraction.vtx` | True vertex |
| `truth` | `vector<size_t>` | Yes | `{interaction_index}` | Index in mc.nu |
| `truthOverlap` | `vector<float>` | Yes | `{1.0}` | 100% overlap (fake reco) |
| `preselected` | `bool` | No | default `false` | |
| **dir** |||||
| `dir.lngtrk` | `SRVector3D` | No | - | Not implemented |
| `dir.heshw` | `SRVector3D` | No | - | Not implemented |
| `dir.calo` | `SRVector3D` | No | - | Not implemented |
| `dir.part_mom_sum` | `SRVector3D` | Yes | Sum of momenta | Direction from sum(p) |
| **nuhyp (CVN scores)** |||||
| `nuhyp.cvn.*` | `float` | No | - | Requires ML/CVN |
| **Enu** |||||
| `Enu.calo` | `float` | Yes | `SRTrueInteraction.E` | True nu energy |
| `Enu.lep_calo` | `float` | No | - | Not implemented |
| `Enu.mu_range` | `float` | No | - | Not implemented |
| `Enu.mu_mcs` | `float` | No | - | Not implemented |
| `Enu.mu_mcs_llhd` | `float` | No | - | Not implemented |
| `Enu.e_calo` | `float` | No | - | Not implemented |
| `Enu.e_had` | `float` | No | - | Not implemented |
| `Enu.mu_had` | `float` | No | - | Not implemented |
| `Enu.regcnn` | `float` | No | - | Not implemented |
| **part.sandreco** |||||
| `part.nsandreco` | `int` | Yes | `nprim` | Number of reco particles |
| `part.sandreco` | `vector<SRRecoParticle>` | Yes | From `prim` | See table below |

**Reco Interaction Summary:** 7 main fields filled

---

## 3. `caf::SRRecoParticle` (Reco Particles)

Reconstructed particles. Stored in `SRInteraction.part.sandreco`.

| Field | Type | Filled | Value | Notes |
|-------|------|--------|-------|-------|
| `primary` | `bool` | Yes | `true` | All primary |
| `pdg` | `int` | Yes | `SRTrueParticle.pdg` | True PDG |
| `tgtA` | `int` | No | default `0` | |
| `score` | `float` | Yes | `1.0` | Perfect PID (fake reco) |
| `E` | `float` | Yes | `SRTrueParticle.p.E()` | True energy [GeV] |
| `E_method` | `PartEMethod` | Yes | `kCalorimetry` | |
| `p` | `SRVector3D` | Yes | `SRTrueParticle.p.Vect()` | True momentum |
| `start` | `SRVector3D` | Yes | `SRTrueParticle.start_pos` | Start position |
| `end` | `SRVector3D` | Yes | `SRTrueParticle.end_pos` | End position |
| `contained` | `bool` | No | default `false` | Requires geometry |
| `walldist` | `float` | No | default `NaN` | Requires geometry |
| `origRecoObjType` | `RecoObjType` | Yes | `kTrack`/`kShower` | From PDG |
| `parent` | `int` | No | default `-1` | |
| `daughters` | `vector<uint>` | No | default `{}` | |
| `truth` | `vector<TrueParticleID>` | Yes | `{ancestor_id}` | Link to MC |
| `truthOverlap` | `vector<float>` | Yes | `{1.0}` | 100% overlap |

**Reco Particle Summary:** 12/16 fields filled

---

## 4. `caf::SRSANDInt` (SAND-specific Reco)

SAND-specific structure. Stored in `standard_record_->nd.sand.ixn`.

| Field | Type | Filled | Value | Notes |
|-------|------|--------|-------|-------|
| `ntracks` | `size_t` | Yes | Count track-like | mu, pi+/-, p, K |
| `tracks` | `vector<SRTrack>` | Yes | From primaries | See table below |
| `nshowers` | `size_t` | Yes | Count shower-like | e+/-, gamma, pi0 |
| `showers` | `vector<SRShower>` | Yes | From primaries | See table below |
| `nclusters` | `size_t` | No | default `0` | Not implemented |
| `ECALClusters` | `vector<SRECALCluster>` | No | default `{}` | Not implemented |

**SAND Interaction Summary:** 4/6 fields filled

---

## 5. `caf::SRTrack` (SAND Tracks)

Reconstructed tracks for charged particles. Stored in `SRSANDInt.tracks`.

| Field | Type | Filled | Value | Notes |
|-------|------|--------|-------|-------|
| `start` | `SRVector3D` | Yes | `SRTrueParticle.start_pos` | |
| `end` | `SRVector3D` | Yes | `SRTrueParticle.end_pos` | |
| `dir` | `SRVector3D` | Yes | `p.Unit()` | Direction from momentum |
| `enddir` | `SRVector3D` | Yes | = `dir` | Same direction |
| `time` | `double` | Yes | `SRTrueParticle.time` | |
| `Evis` | `float` | Yes | `p.E() * 1000` | Energy [MeV] |
| `qual` | `float` | Yes | `1.0` | Perfect quality |
| `charge` | `short int` | Yes | From PDG | +1, 0, -1 |
| `len_gcm2` | `float` | No | default `NaN` | Requires material info |
| `len_cm` | `float` | Yes | `|end - start|` | Geometric length |
| `E` | `float` | Yes | `p.E() * 1000` | Energy [MeV] |
| `truth` | `vector<TrueParticleID>` | Yes | `{ancestor_id}` | Link to MC |
| `truthOverlap` | `vector<float>` | Yes | `{1.0}` | 100% overlap |

**Track Summary:** 12/13 fields filled

---

## 6. `caf::SRShower` (SAND Showers)

Reconstructed showers for EM particles. Stored in `SRSANDInt.showers`.

| Field | Type | Filled | Value | Notes |
|-------|------|--------|-------|-------|
| `start` | `SRVector3D` | Yes | `SRTrueParticle.start_pos` | |
| `direction` | `SRVector3D` | Yes | `p.Unit()` | Direction from momentum |
| `Evis` | `float` | Yes | `p.E() * 1000` | Energy [MeV] |
| `truth` | `vector<TrueParticleID>` | Yes | `{ancestor_id}` | Link to MC |
| `truthOverlap` | `vector<float>` | Yes | `{1.0}` | 100% overlap |

**Shower Summary:** 5/5 fields filled (100%)

---

## PDG Classification

### Track-like (stable charged particles)
| PDG | Particle |
|-----|----------|
| +/-13 | muon |
| +/-211 | charged pion |
| +/-321 | charged kaon |
| 2212 | proton |

### Shower-like (EM particles)
| PDG | Particle |
|-----|----------|
| +/-11 | electron/positron |
| 22 | photon |
| 111 | pi0 |

---

## Fields NOT filled (and why)

### Truth (`SRTrueInteraction`)
1. `removalE` - Requires specific GENIE info not available
2. `t` - Only for coherent/diffractive scattering, not implemented
3. `baseline`, `prod_vtx`, `parent_dcy_*`, `imp_weight` - Require NuParent data from flux driver
4. `genVersion` - Not implemented

### Reco (`SRInteraction`)
1. `dir.lngtrk`, `dir.heshw`, `dir.calo` - Require real reco algorithms
2. `nuhyp.cvn.*` - Require CVN neural network
3. `Enu.*` (except `calo`) - Require specific reco algorithms

### Particles (`SRRecoParticle`)
1. `contained`, `walldist` - Require detector geometry
2. `tgtA` - Not needed for fake reco

### SAND (`SRSANDInt`)
1. `ECALClusters` - Requires separate ECAL simulation

---

## Data Sources

### GENIE EvtCode
Parsing of the `Interaction::AsString()` string to extract interaction type, PDG, etc.
Parser implemented in `genie_helpers/EvtCode_parser.hpp`.

### GENIE StdHep
Array of particles in HepMC format:
- Index 0: neutrino
- Index 1: nuclear target
- Subsequent indices: interaction products

### edep-sim
Tracks propagated by GEANT4:
- **Primaries**: direct children of the interaction vertex (post-FSI)
- **Secondaries**: particles produced by propagation of primaries

---

## Related Files

- `fake_reco.cpp` - Main process implementation
- `fake_reco.hpp` - Header with declarations
- `caf_handlers/interactions.hpp` - Functions for `SRTrueInteraction`
- `caf_handlers/particles.hpp` - Functions for `SRTrueParticle`
- `caf_handlers/tracks_showers.hpp` - Functions for `SRTrack`, `SRShower`, `SRRecoParticle`
- `genie_helpers/EvtCode_parser.hpp` - Parser for GENIE EvtCode string
