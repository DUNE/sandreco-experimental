# 900_fast_reco test

Consumes the `SRTruthBranch`, `SRCommonRecoBranch` and `SRNDBranch` produced by
`sand::common::truth_filler`/`sand::common::fast_reco` and checks that the output is
internally consistent and round-trip navigable, not just crash-free.

Interaction-level:

- `common.ixn.nsandreco`/`nd.sand.nixn` match `common.ixn.sandreco.size()`/`nd.sand.ixn.size()`.
- `common.ixn.sandreco`, `nd.sand.ixn` and `truth.nu` are the same size (one `SRInteraction`/
  `SRSANDInt` per `SRTrueInteraction`, built in lockstep by `fast_reco`).

For every `SRRecoParticle` in `part.sandreco[i]`:

- `part.nsandreco`/`tracker.ntracks`/`tracker.nshowers` match the corresponding vector sizes.
- **Truth match**: `truth[0]` resolves (via `truth.nu[ixn].prim`/`.sec`) to a real
  `SRTrueParticle` whose `pdg` matches the reco particle's `pdg`.
- **`recoobj` <-> `part` round trip**: if `origRecoObjType == kTrack`, `recoobj` points to a
  valid `SRTrack` in `sand_ixn.tracker.tracks`, and that track's own `part` points back to this
  exact particle (same `ixn`/`ipart`). Symmetric check for `kShower` against `tracker.showers`.
- **`parent`/`daughters` round trip**: if `parent >= 0`, the parent particle's `daughters`
  contains this particle's own index.

Wired into `900_fast_reco_test.json` between `fast_reco` and `caf_streamer`; any violation
aborts via `UFW_ASSERT`, failing the ctest.

Run: `ctest --test-dir build -R 900_fast_reco_test --output-on-failure` (after building and
**installing** `sand_test_fast_reco`, since `ufwrun` loads plugins from `/usr/local/lib64`).
