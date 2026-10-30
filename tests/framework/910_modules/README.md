# 910_truth_filler test

Consumes the `SRTruthBranch` produced by `sand::common::truth_filler` and checks that
`build_true_particle_tree()`'s output is internally consistent, not just crash-free.

For every interaction (`nu[i]`):

- `nprim`/`nsec` match `prim.size()`/`sec.size()`.
- `nproton`/`nneutron`/`npip`/`npim`/`npi0` match counts recomputed from `prim`'s PDGs.
- Every **primary**'s `ancestor_id` points to itself, `parentID` is unset (`kUnknown`),
  and each of its `daughtersID` resolves to a secondary whose `parentID`/`ancestor_id`
  point back correctly.
- Every **secondary**'s `ancestor_id` resolves to a valid primary, `parentID` resolves to
  a valid node in the same interaction, the pre-order invariant holds (a secondary parent
  is always inserted before its children), and the round trip holds: the parent's
  `daughtersID` lists this node back.
- `daughters` and `daughtersID` have matching sizes.
- **Track ids**: `G4IdMap::to_spill_id(G4ID)` (maps from `sand::spill_maps`, the same framework data
  `truth_filler` used) resolves to the edep-sim trajectory the particle was
  built from: same pdg, same start point, and its parent is `to_spill_id(parent)`. At least one
  spill must have more than one interaction, otherwise interaction and spill ids coincide and this
  check would pass even with a wrong conversion.

Wired into `910_truth_filler.json` between `truth_filler` and `caf_streamer`; any
violation aborts via `UFW_ASSERT`, failing the ctest.

Run: `ctest --test-dir build -R 910_truth_filler --output-on-failure` (after building and
**installing** `sand_test_truth_filler`, since `ufwrun` loads plugins from `/usr/local/lib64`).
