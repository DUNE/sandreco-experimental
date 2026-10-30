# tracker::fast_reco

Fast reconstruction of the SAND tracker: one `SRTrack` or `SRShower` per true particle that left enough energy in
the tracker, built from the particle's edep-sim hits and trajectory points in the tracker volumes. Writes the
spill-level `tracker_reco_wrapper` (`out_tracker`); `sand::common::fast_matching` then links each object to its
`SRRecoParticle` and copies the collection to `nd.sand.tracker`.

"Fast" means cheated pattern recognition: the hits of each particle are known from the truth, there is no
digitization and no fit. The kinematics is exact; resolution is added downstream by `gauss_smearing` /
`gluckstern_smearing`.

## Configuration

```json
{
  "sand::tracker::fast_reco": { "min_energy_deposit": 0.04 },
  "reqs": { "in_truth": "produced_truth" },
  "prods": { "out_tracker": "produced_tracker" }
}
```

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `min_energy_deposit` | MeV | `0` | Minimum energy a particle must deposit in the tracker to get an object |

It also needs two framework data in `contexts.locals`: `sand::edep_reader` (the hits) and `sand::spill_maps` (to go
from `SRTrueParticle::G4ID` to the edep-sim trajectory, see `src/data/common/spill_maps/README.md`).

## Which particles get an object

For every true particle of the spill (`truth.nu[i].prim`, then `.sec`), with its edep-sim trajectory:

1. **Hits in the tracker.** The hits in `subdetector_t::STT` and `subdetector_t::DRIFT` (the two tracker
   technologies; which one a file has depends on the simulated geometry). No hits: no object.
2. **Energy threshold.** The energy deposited by those hits must be at least `min_energy_deposit`. The configs of
   tests 500 and 900–902 use 0.04 MeV: on the test sample photons own at most one hit of a few keV (max 31 keV),
   while the least energetic charged pion deposits 49 keV.
3. **Track or shower.** Track-like PDG (μ±, π±, K±, p/p̄) → `SRTrack`; shower-like (e±, γ, π⁰) → `SRShower`; any
   other PDG gets nothing.

The threshold is inside the reco, not in a `ufw::filter`: a filter decides whether a whole spill is written, not
which objects are kept.

Each step is counted and logged once per spill (debug level):

```
tracker::fast_reco: 17 tracks, 22 showers; 7238 particles without tracker hits, 172 below min_energy_deposit
(0.04 MeV), 0 above it but neither track- nor shower-like; 0 objects without trajectory points in the tracker
```

A true particle without an edep-sim trajectory is an error (`UFW_ERROR`), not a skipped particle.

## `SRTrack`

The tracker sees the particle only inside the tracker: positions, time and length come from its tracker hits,
directions and energy from its trajectory points in the tracker.

| Field | Source | Unit |
|---|---|---|
| `start` | start of the earliest tracker hit | cm |
| `end` | stop of the latest tracker hit | cm |
| `time` | start time of the earliest tracker hit | ns |
| `len_cm` | sum of the tracker hit lengths (path length in the tracker, not the chord) | cm |
| `Evis` | energy deposited by the tracker hits | MeV |
| `dir` | momentum direction at the first trajectory point in the tracker | |
| `enddir` | momentum direction at the last trajectory point in the tracker | |
| `E` | `sqrt(p² + m²)` at the first trajectory point in the tracker (the energy a curvature fit would see) | MeV |
| `charge` | sign of the PDG charge (`TDatabasePDG`), `0` if unknown | |
| `qual` | `1` | |
| `truth`, `truthOverlap` | `{TrueParticleID{ixn, kPrimary\|kSecondary, index}}`, `{1}` | |
| `part` | set by `fast_matching` | |

Not filled: `len_gcm2`.

Special cases of the directions and energy:

- **Particle stopping in the tracker**: its momentum at the last point is zero, so `enddir` goes from the
  second-to-last to the last point.
- **No trajectory point in the tracker** (edep-sim does not store a point at every step): `dir` and `enddir` are the
  directions of the first and last hit, and `E` is NaN. The log line counts these objects.

## `SRShower`

| Field | Source | Unit |
|---|---|---|
| `start` | start of the earliest tracker hit | cm |
| `time` | start time of the earliest tracker hit | ns |
| `Evis` | energy deposited by the tracker hits | MeV |
| `direction` | momentum direction at the first trajectory point in the tracker (first hit if none) | |
| `truth`, `truthOverlap` | as for tracks | |
| `part` | set by `fast_matching` | |

Not filled: `qual`, `len_cm`, `initial_dEdx`, `conversionGap` (they need an actual shower reconstruction).

## Code layout

| File | Namespace | Content |
|---|---|---|
| `fast_reco.hpp/.cpp` | `sand::tracker` | the process: configuration, tracker volumes, threshold, track/shower choice, log |
| `common_utils.hpp/.cpp` | `sand::fast_reco` | what does not depend on the subdetector, meant to be shared with the GRAIN and ECAL fast reco |

`common_utils`:

- `VolumeHits`: what one trajectory leaves in a set of volumes, i.e. its hits ordered by time and its first, last and
  second-to-last trajectory point there. A self-contained value: hits and points are copies, nothing points into
  the `EDEPTree`. Built by `volume_hits_through(trajectory, volumes)` or, from the hit and point maps,
  `volume_hits_in(hit_map, points, volumes)`.
- `energy_deposit`, `track_length`, `entry_direction`, `exit_direction`.
- `track_from` / `shower_from`: the `SRTrack` / `SRShower` of the tables above, from a `VolumeHits`.
- `is_track_like` / `is_shower_like`: the PDG lists above.
- `for_each_true_particle(truth, edep, spill_maps, f)`: calls `f(true_particle, id, trajectory)` for every true
  particle of the spill.

A point lying in a volume that is both STT and drift is stored once per subdetector by `edep_reader`;
`volume_hits_*` skips the copy, otherwise the second-to-last point would coincide with the last one.

## Tests

- `tests/standalone/tracker_fast_reco`: values of `track_from` / `shower_from` and the hit selection, on hand-made
  hits and points.
- `tests/framework/500_tracker_fast_reco.json`: the process alone.
- `tests/framework/900_fast_reco.json` (`900_modules`): the whole chain, re-deriving the gate and the object fields
  independently from edep.
