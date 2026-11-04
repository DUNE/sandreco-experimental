# sandreco — Work Recap (Nov 2025 – Oct 2026)

A high-level summary of the work merged into `main` since November 2025, and the
recent work in flight. The repo evolved from a mostly skeleton reconstruction
framework into a working, subdetector-organized reconstruction framework with
releases **v1.0.0 … v1.0.8** (April–October 2026).

---

## 1. Architecture evolution

### 1.1 Data model: from ad-hoc structures to a consistent reco object model
- **Common reco types** (Dec 2025): introduced the generic `sand::reco` data
  model — `reco::digi` (with a general collection of truth-derived objects
  attached, Mar 2026), `reco::hit`, energy deposition types and helpers.
- **Track class** (Jul 2026): added `sand::reco::track` — an ordered sequence
  of hits/segments with begin/end, length and time-span accumulation
  utilities, plus a fix for hit inheritance in grain clusters.
- **Generic scalar/vector data structures** (May 2026): general-purpose
  managed data types for scalar and vector payloads.
- **Managed-data conventions**: voxels got first-class managed data
  (`spill_data_collection` + `SAND_DATA_*_COLLECTION` macros), and a
  `point_clusters` type for Hough/clustering output.
- **CAF format** (Mar 2026): sand CAFs were adapted to match the DUNE CAF
  format, with a CAF wrapper and caf streamer; CAF wrappers for grain, tracker
  and ECAL reco outputs were added in Oct 2026.

### 1.2 Geometry and geoinfo
- ECAL: geoinfo module (Feb 2026), Al plate handling in `ecal_info`
  (Feb 2026), target/station size fix (Jul 2026), ECAL cell **neighbour map**
  (Oct 2026).
- Grain: detector-response geoinfo (Nov 2025), **rectangular camera** support
  (Oct 2026) — the whole grain chain (slicing, digitalization, reco) now works
  with rectangular masks.
- Drift: **geoinfo for generic drift geometry** (Jul 2026) and fixes to
  volume-name parsing for generic drift geometries (Oct 2026).
- BVH algorithm for cell adjacency (Mar 2026).

### 1.3 Input readers (simulation side)
- `edep_reader` (edep-sim): reads primaries, `root_tgeomanager`, trajectory
  points, extra components in the edep tree, event-based files, per-interaction
  track IDs.
- `genie_reader`: refactored to the default reader, **GHEP support** and
  event-based edep-sim files (Oct 2026), correct GENIE *t* computation, public
  headers.
- `G4IdMap` (Oct 2026): maps G4IDs to edep-sim trajectories so that
  multi-interaction spills are resolved to the right trajectory by
  `fast_reco`, `gluckstern_smearing` and `truth_filler`.

### 1.4 I/O
- **HDF5 support** (Dec 2025) alongside ROOT; root streamer branch naming
  fixes (May 2026), test coverage for reading multiple variables from the same
  streamer (Jul 2026).
- Grain images: dropped ROOT custom types in favor of standard streamable
  types; **timeslice output** added to the grain slicer (Oct 2026); PNG
  streamer for camera images.

### 1.5 Build, CI and documentation
- CMake reorganization + **versioning** (Apr 2026), first release tags
  (`v1.0.0`…`v1.0.8`), fully-qualified library names, missing install rules
  for ROOT dictionaries/rootmaps fixed (Mar 2026).
- CI: doxygen pipeline added and improved (Mar 2026), **automated doc tools**
  (Sep 2026), CI on the GitHub mirror (Mar 2026), fail-fast on build errors
  (Jul 2026), **tests reorganized into parallel, dependency-aware groups**
  (Oct 2026) to speed up CI.
- Test infrastructure: test-module helpers (Apr 2026), edep-partitioning test
  helpers, geoinfo and `detector_response` tests, digitization tests for the
  tracker.

---

## 2. New modules and algorithms by area

### 2.1 GRAIN (scintillator)
The most active subdetector: a full chain was built and refined.

| Area | What was added/evolved |
|---|---|
| Digitalization | `fast_digi` (fast grain digitization), grain digi types adapted for spill-based edep-sim (Oct 2026) |
| Slicing | `spill_slicer` (placeholder → real spill handling, Apr 2026), **timeslice output** (Oct 2026) |
| Optical simulation | Optical simulation fixes (Oct 2026), energy deposits API aligned with edep-sim |
| Clustering/track finding | Port of **volumetric reco** (Apr 2026) with follow-up debugging; **mask weights computation** (Jan 2026, GPU OpenCL); **voxels → 3D point cloud** (May 2026); port of **clustering** (May 2026); **3D Hough transform on GPU** (Jun 2026, OpenCL); **fit of Hough-clustered points** to a track line (Jul 2026); `detector_response_fast` |
| Mask calorimetry | `mask_voxel_calorimetry` — deposited energy from mask reconstruction (Apr 2026) |
| Geometry | Rectangular cameras (Oct 2026) |

### 2.2 ECAL
Built from scratch during this period:

- **geoinfo** for ECAL (Feb 2026) incl. Al plate handling
- **Digitization** module (Feb 2026)
- **(Raw-)hit reconstruction** (Mar & Sep 2026): `optical_simulation`,
  `cell_pair_builder`, `reco_cell_builder`
- Spill slicer placeholder (Mar 2026)
- **Neighbour map** for ECAL cells (Oct 2026)
- ECAL image/slice and photoelectron/digit types in `include/`

### 2.3 Tracker / STT / Drift
- **Fast STT digitization** (Nov 2025) with channel-ID matching (Jan 2026)
- **Fast drift digitization** (Dec 2025)
- **Clustering** ported (May 2026); **weight reporting** (Jun 2026)
- Digitization tests for the tracker (Jun 2026)
- Adaptation of `fast_reco`/`gluckstern_smearing` to **spill-level SRSAND
  tracker** (Oct 2026)
- `track_in_detector` filter

### 2.4 Common reconstruction flow
- **fake_reco → fast_reco** (Sep 2026): the "fast" reconstruction (reco from
  edep-sim trajectories) was renamed, the final track direction is now
  calculated from the edep, and it was **split per subdetector**
  (Oct 2026) so each subdetector consumes its own trajectories.
- **truth_filler / fast_fake_reco separation** (Jul 2026): truth and reco
  filling were split into dedicated modules.
- **Smearing** (Jun 2026): gaussian smearing (preserving particle mass by
  deriving E from smeared p) and **Gluckstern smearing** (level arm as path
  length, preserving trajectory end direction).
- **Filters**: `edepfilter` class (Jun 2026), `primary_nu` filter extended
  with **neutrino type and energy-range selection** (Oct 2026), dark-neutrino
  support in fake_reco (May 2026), secondary particles in fake_reco (Jun 2026).
- Fiducial-volume energy fix (Oct 2026).

### 2.5 GPU / OpenCL
- Generic **OpenCL support library** (Nov 2025)
- Mask weights computation on GPU (Jan 2026)
- **3D Hough transform on GPU** (Jun 2026) — GPU kernel + point-cloud
  conversion utilities

---

## 3. Timeline (merge highlights)

- **Nov 2025** — grain implementation (core), grain geoinfo/detresponse,
  fast STT digitization, OpenCL support library, clang-format, ufw 1.8
- **Dec 2025** — edep_reader maturation (primaries, tgeomanager,
  trajectories), HDF5 support, common reco types, fast drift digitization,
  print helpers
- **Jan 2026** — mask weights computation, STT channel-ID matching,
  `tLorentzVector` → `xyztVector` migration, test reorganization
- **Feb 2026** — ECAL geoinfo + plate handling, ECAL digitization,
  duneanaobj integration, skeleton implementation module
- **Mar 2026** — ECAL (raw-)hit reconstruction, spill slicer placeholder,
  sand-CAF format, BVH cell adjacency, doxygen in CI, GitHub-mirror CI
- **Apr 2026** — CMake reorganization + versioning (v1.0.0), volumereco port,
  spill handling in slicer/volumereco, mask calorimetry, voxels data,
  voxels→point-cloud, test-module infrastructure
- **May 2026** — clustering port, dark neutrinos in fake_reco, detector
  response & spill slicer tests, geoinfo testing
- **Jun 2026** — smearing into fake_reco (gauss + gluckstern), edepfilter,
  secondary particles, tracker weight reporting & digitization tests,
  **3D Hough on GPU**
- **Jul 2026** — track class, grain cluster hit-inheritance fix, generic-drift
  geoinfo, hough3d point fitting, truth/reco filler split, fail-fast CI
- **Sep 2026** — ECAL cell_hit reconstruction, automated documentation tools,
  public genie_reader headers
- **Oct 2026** — rectangular cameras, ECAL neighbour map, timeslice output,
  fiducial energy fix, grain optical sim fixes, fake_reco → fast_reco,
  genie_reader GHEP/event-based, neutrino type/energy filters, generic-drift
  volume names, fast_reco subdetector split + G4IdMap, test reorganization
  (v1.0.8)

---

## 4. Recent work (in flight, not yet merged)

Branch `144-refine-hit-class-and-track-class-add-vertex-class`:

- **New `sand::reco::shared` base class for quota-based ownership**
  (`include/common/shared.h`): an object of a derived class represents a
  physical object (hit, cluster, track, ...) with a *quota* — the fraction of
  the physical object the instance owns; live instances of the same physical
  object sum to quota 1.0. Objects are created with the static
  `shared::create<Child>(args...)` (quota 1.0) and may only be duplicated by
  the static `shared::split(x, frac)`, which consumes `x` and returns two
  copies with quotas scaled by `frac` and `1 - frac`
  (`UFW_ASSERT(0 < frac < 1)`). Copy/move constructors and assignments are
  suppressed throughout the hierarchy (children declare a `protected` copy
  constructor and a private `copy_self()` used by split); a public
  `is_unique()` reports `quota == 1.0`; a protected default constructor
  initializes the quota to NaN for ROOT deserialization.
- **`reco::hit` and `reco::track` are the first `shared` children**: `hit`
  keeps its kinematic payload; `track` stores `std::vector<std::unique_ptr<hit>>`,
  pushes hits by move, and its `accumulate_on_hits` callback now receives
  `const hit&`. Splitting a track deep-copies its hits while preserving their
  quotas. A new `SAND_DATA_SHARED_COLLECTION` macro /
  `spill_data_collection_shared` managed-data type (element
  `std::unique_ptr<T>`) is introduced, and `sand::reco::tracks` is declared.
- Grain `point_clusters::cluster` becomes a `reco::hit` subclass held via
  `unique_ptr`; `point_clusters_list` is a `vector<vector<unique_ptr<cluster>>>`
  and `hough3d` / `fit_cluster` were adapted to emit such outputs (ROOT 6.36
  streams `unique_ptr` TTree members; verified by framework tests 182/183).
- Tests: new standalone `shared` unit test (create/split/nested split/
  copy-suppression/NaN default ctor) and extended `track` unit test covering
  hit-splitting across tracks and quota-preserving track splits.
- Planned next step per the branch name: a **vertex class** built on the
  same shared-object model.
