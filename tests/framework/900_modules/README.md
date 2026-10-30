# 900_fast_reco test

Consumes the `SRTruthBranch`, `SRCommonRecoBranch` and `SRNDBranch` produced by
`sand::common::truth_filler` → `sand::tracker::fast_reco` → `sand::common::fast_matching` and checks that
the output is internally consistent and round-trip navigable, not just crash-free.

Interaction-level:

- `common.ixn.nsandreco`/`nd.sand.nixn` match `common.ixn.sandreco.size()`/`nd.sand.ixn.size()`.
- `common.ixn.sandreco`, `nd.sand.ixn` and `truth.nu` are the same size (one `SRInteraction`/
  `SRSANDInt` per `SRTrueInteraction`, built in lockstep by `fast_matching`).
- `nd.sand.tracker.ntracks`/`nshowers` match the corresponding vector sizes (the tracker
  collections are spill-level, shared by every interaction).
- Every track/shower in `nd.sand.tracker` is claimed by exactly one `SRRecoParticle` over the spill.

For every `SRRecoParticle` in `part.sandreco[i]`:

- `part.nsandreco` matches the corresponding vector size.
- **Truth match**: `truth[0]` resolves (via `truth.nu[ixn].prim`/`.sec`) to a real
  `SRTrueParticle` whose `pdg` matches the reco particle's `pdg`.
- **Trajectory lookup**: the true particle's `G4ID`, converted with `G4IdMap::to_spill_id()` (maps from
  `sand::spill_maps`), resolves to an edep-sim trajectory with the same `pdg` and start point. The test
  then uses that same trajectory to re-derive which particles should pass the tracker gate (hits in
  STT/drift and at least `min_energy_deposit`, which the test receives with the same value as the reco)
  and each object's fields.
- **`recoobj` <-> `part` round trip**: if `origRecoObjType == kTrack`, `recoobj` points to a
  valid `SRTrack` in `nd.sand.tracker.tracks`, and that track's own `part` points back to this
  exact particle (same `ixn`/`ipart`). Symmetric check for `kShower` against `tracker.showers`.
- **`parent`/`daughters` round trip**: if `parent >= 0`, the parent particle's `daughters`
  contains this particle's own index.
- **Units**: `SRRecoParticle::E` equals the true particle's `SRTrueParticle::p.E` (both GeV).
  `SRTrack::E` is in MeV and never larger than the true energy at the vertex.
- **Entry/exit kinematics**: a track's `E` and `dir` are the energy and momentum direction at its
  first trajectory point in the tracker, `enddir` the momentum direction at the last one; a
  shower's `direction` the one at the first. Without trajectory points in the tracker, directions
  come from the first/last hit segment and the track `E` is NaN (the job reports how many).
- **Tracker objects come from the tracker hits only**: `start` is the start of the earliest tracker
  hit (STT/drift), `end` the stop of the latest, `time` the time of the earliest; `len_cm` is the
  summed length of the tracker hits and `Evis` the energy deposited in the tracker (MeV). Same for
  showers (`start`, `time`, `Evis`). Re-derived here without sorting, independently of the reco.

Wired into `900_fast_reco.json` between `fast_matching` and `caf_streamer`; any violation
aborts via `UFW_ASSERT`, failing the ctest.

Run: `ctest --test-dir build -R 900_fast_reco --output-on-failure` (after building and
**installing** `sand_test_fast_reco`, since `ufwrun` loads plugins from `/usr/local/lib64`).
