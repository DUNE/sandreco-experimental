# 900_fast_reco test

Consumes the `SRTruthBranch`, `SRCommonRecoBranch` and `SRNDBranch` produced by
`sand::common::truth_filler`/`sand::common::fast_reco` and checks that the output is
internally consistent and round-trip navigable, not just crash-free.

Interaction-level:

- `common.ixn.nsandreco`/`nd.sand.nixn` match `common.ixn.sandreco.size()`/`nd.sand.ixn.size()`.
- `common.ixn.sandreco`, `nd.sand.ixn` and `truth.nu` are the same size (one `SRInteraction`/
  `SRSANDInt` per `SRTrueInteraction`, built in lockstep by `fast_reco`).
- `nd.sand.tracker.ntracks`/`nshowers` match the corresponding vector sizes (the tracker
  collections are spill-level, shared by every interaction).
- Every track/shower in `nd.sand.tracker` is claimed by exactly one `SRRecoParticle` over the spill.

For every `SRRecoParticle` in `part.sandreco[i]`:

- `part.nsandreco` matches the corresponding vector size.
- **Truth match**: `truth[0]` resolves (via `truth.nu[ixn].prim`/`.sec`) to a real
  `SRTrueParticle` whose `pdg` matches the reco particle's `pdg`.
- **`recoobj` <-> `part` round trip**: if `origRecoObjType == kTrack`, `recoobj` points to a
  valid `SRTrack` in `nd.sand.tracker.tracks`, and that track's own `part` points back to this
  exact particle (same `ixn`/`ipart`). Symmetric check for `kShower` against `tracker.showers`.
- **`parent`/`daughters` round trip**: if `parent >= 0`, the parent particle's `daughters`
  contains this particle's own index.
- **Units**: `SRRecoParticle::E`, and `SRTrack::E`/`Evis`/`SRShower::Evis` when there's a
  matching track/shower, all equal the true particle's `SRTrueParticle::p.E` exactly (GeV,
  fast_reco applies no smearing). Catches a stray unit conversion (e.g. an accidental
  GeV->MeV factor) that would otherwise pass every structural check above unnoticed.

Wired into `900_fast_reco_test.json` between `fast_reco` and `caf_streamer`; any violation
aborts via `UFW_ASSERT`, failing the ctest.

Run: `ctest --test-dir build -R 900_fast_reco_test --output-on-failure` (after building and
**installing** `sand_test_fast_reco`, since `ufwrun` loads plugins from `/usr/local/lib64`).
