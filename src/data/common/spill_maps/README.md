# spill_maps

Conversion between the two ids of an edep-sim trajectory, as framework data built once per spill.

## The two ids

- **spill id**: the Geant4 track id from edep-sim, unique over the whole spill. This is what
  `EDEPTree::GetTrajectory()` expects.
- **interaction id**: what ends up in `SRTrueParticle::G4ID`, `parent` and `daughters`. It restarts from 0 in every
  interaction: primaries first (`0..nprim-1`), then secondaries (all the descendants of the primaries), each group
  in increasing spill id, as in ND_CAFMaker.

For example, an interaction whose primaries have spill ids 2, 3, 4 and whose secondaries have spill ids 8, 9, 10
gets interaction ids 0, 1, 2 and 3, 4, 5.

**Never pass `SRTrueParticle::G4ID` to `GetTrajectory()` directly**: it only works by chance for the primaries of the
first interaction, and silently returns another particle's trajectory otherwise.

## Usage

In a process:

```cpp
#include <spill_maps/spill_maps.hpp>

auto const& edep = instance<sand::edep_reader>();
auto const& maps = instance<sand::spill_maps>();   // by reference: framework data cannot be copied
auto const trj   = edep.GetTrajectory(maps.at(ixn).to_spill_id(true_part.G4ID));
```

`maps.at(ixn)` is the `G4IdMap` of interaction `ixn`, indexed like `SRTruthBranch::nu`.

In the job configuration, next to `sand::edep_reader` (ufw requires an entry for every context data, even with an
empty configuration):

```json
"contexts": {
  "locals": {
    "sand::edep_reader": { "uri": "..." },
    "sand::spill_maps": {}
  }
}
```

In CMake: `target_link_libraries(<target> PRIVATE sand_spill_maps)`.

## Files

| File | Content | Depends on |
|---|---|---|
| `id_range.hpp` | `IdRange` (half-open range of ids) and `IdBounds` (min, max, count of a set of ids), with `id_bounds_of`, `merge_id_bounds`, `contiguous_id_range`; all `constexpr` | standard library only |
| `g4_id_map.hpp/.cpp` | `G4IdMap`, `make_g4_id_map`, `g4_id_maps`; `InteractionRange`, `make_interaction_ranges` | edep-sim trajectories |
| `spill_maps.hpp/.cpp` | the framework data `sand::spill_maps` and its factory | ufw, `sand::edep_reader` |

`G4IdMap` can be built and tested without the framework: construct it from two `IdRange`s, or from the trajectory
tree with `make_g4_id_map`.

## How the maps are built

- `make_interaction_ranges` splits the top-level trajectories of the spill into interactions with
  `EDEPTrajectory::GetInteractionNumber()`. The primaries of one interaction must be adjacent, as edep-sim writes
  them; an interaction split in two blocks would give two ranges.
- Within an interaction, the spill ids of the primaries and those of their descendants are each **contiguous**. A
  `G4IdMap` is therefore two `IdRange`s (first id, count), and both conversions are an offset. `make_g4_id_map`
  checks the contiguity while walking the tree (min, max and count of the ids) and stops with `UFW_ERROR`, naming
  the interaction, if it does not hold.
- The factory of `sand::spill_maps` rebuilds the maps only when the spill changes: every process asking for them in
  the same spill gets the same object.

Users: `truth_filler` (`G4ID`, `parent`, `daughters` of the true particles), `tracker::fast_reco`,
`gluckstern_smearing`, the 900 and 910 framework tests; `primary_nu` uses `make_interaction_ranges` only.
