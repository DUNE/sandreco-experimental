# Geometry information (`sand::geoinfo`)

`sand::geoinfo` (library `sand_geoinfo`, `src/data/common/geoinfo`) is the global complex data that describes the
SAND geometry loaded by `sand::root_tgeomanager`: the subdetector descriptions (`ecal()`, `grain()`, `tracker()`),
`get_info()` for the subdetector at a point, and the magnetic field. It is configured in the `globals` section of a
job:

```json
"globals" : {
  "sand::root_tgeomanager" : { "geometry" : "test/SAND_opt3_DRIFT1.sand-events-in-sand_inner_volume.2.edep.root" },
  "sand::geoinfo" : { "field" : { "source" : "gdml", "file" : "geometries/sand/SAND_opt3_DRIFT1.gdml" } }
}
```

Units: lengths in mm in the global frame of the loaded geometry, as written by edep-sim.

## Magnetic field

### Interface

Declared in `geoinfo/geoinfo.hpp`, with the field classes in `geoinfo/field_info.hpp`:

| Member                                   | Description                                                                  |
|------------------------------------------|------------------------------------------------------------------------------|
| `dir_3d field_at(const pos_3d&) const`   | B in tesla at a global position in mm; zero outside the world                |
| `field_probe make_field_probe() const`   | a new probe, the fast lookup object to be kept by the caller (see below)     |
| `bool has_field() const`                 | whether a field is configured                                                |
| `const field_info& field() const`        | the field description: `uniform_field` or `volume_field`                     |

`field_at`, `make_field_probe` and `field` throw `geoinfo::field_not_configured` when the configuration has no
`"field"` key; the message says how to add one.

```cpp
#include <geoinfo/field_info.hpp>

auto const& gi = instance<sand::geoinfo>();
sand::dir_3d b = gi.field_at(sand::pos_3d(0., -2384.73, 23910.)); // (0.6, 0, 0) T in the tracker

// in a process, keep a probe as a member and bind it once
sand::geoinfo::field_probe m_probe;
...
if (!m_probe) {
  m_probe = gi.make_field_probe();
}
sand::dir_3d b = m_probe.field_at(p);
```

### Configuration

The `"field"` key of the `sand::geoinfo` configuration takes one of two forms:

- `[Bx, By, Bz]`: uniform field, components in tesla (`geoinfo::uniform_field`);
- `{ "source" : "gdml", "file" : <path> }`: field of the geometry volumes, from the `BField` tags of the GDML
  (`geoinfo::volume_field`). A relative path is resolved against `ufw-basepath`, as the geometry of
  `root_tgeomanager`. The GDML must be the one the loaded geometry was simulated from.

Without the key there is no field, and jobs that do not ask for it behave as before. Anything else is an error that
names the offending key or value: unknown keys in the object (`"files"`), a `source` other than `"gdml"`, a missing
or non-string `file`, an array that is not three numbers, a value that is neither an array nor an object. The field
is built when `geoinfo` is constructed, so these errors, an unreadable GDML and tags that do not match the geometry
are reported when the job starts.

### Field of the geometry volumes

**Source.** The SAND GDML defines the field with auxiliary tags on volumes, in the format read by edep-sim:

```xml
<volume name="MagIntVol_volume">
  <auxiliary auxtype="BField" auxvalue="(0.600000 T, 0.0 T, 0.0 T)"/>
```

In `nd_hall_with_lar_tms_sand_drift1_v2026.03.06.gdml` there are nine tags: `MagIntVol_volume`, the volume containing
the calorimeter and the tracker, with 0.6 T along x; `KLOEYokeBarrel_volume` with −1.746067 T along x; six TMS
volumes with ±1 T along y; `volTPCActive` with an `EField` only. The yoke endcaps have no tag.

**Why the GDML is read as XML.** The TGeoManager loaded by `root_tgeomanager`, from an edep-sim file or from a
geometry ROOT file, does not keep the GDML auxiliaries. Importing the GDML again with ROOT is not an option: ROOT
allows one TGeoManager per process (a second import replaces `gGeoManager`), and a GDML imported by ROOT is in ROOT
units (cm) instead of the mm of the edep-sim geometry. So `read_gdml_field_tags(path)` reads only the `<volume>`
elements and their `BField`/`EField` auxiliaries, with ROOT's XML parser (`TXMLEngine`), and builds no geometry.
`volume_field` matches them to the loaded volumes by name: GDML volume `V` is the TGeoVolume `V` or, as edep-sim
writes it, `V_PV`. A tag whose volume is in neither form is an error. The components are numbers with an optional
unit (`T`, `tesla`, `kG`, `kilogauss`, `G`, `gauss`; tesla when missing), with or without the surrounding
parentheses. External XML entities are not expanded.

**Inheritance.** The rule is the one of edep-sim, which attaches a field manager to each tagged logical volume and
forces it onto all its daughters, visiting the tree from the top:

- a volume has the field of its nearest tagged ancestor, itself included: the tracker, with no tag of its own, has
  the 0.6 T of `MagIntVol_volume`;
- a volume with an `EField` tag and no `BField` tag has B = 0 (edep-sim gives it a field manager of its own, with
  zero magnetic field), which hides the field of its ancestors;
- a volume without a tagged ancestor, and any point outside the world, has zero field;
- the field belongs to the logical volume, shared by all its placements. A volume that would inherit two different
  fields through different placements is an error, since in edep-sim the result would depend on the visiting order.
  In the SAND geometry every logical volume has a single mother, so this does not occur.

**Setup summary.** The table of the field of each logical volume is computed once, when `geoinfo` is built, and
summarised in the log at info level: one line per tag with its GDML volume, the matching volume of the geometry, its
field and the number of logical volumes inheriting it (or "unused" if the volume is not placed in the tree), and the
number of volumes with no tagged ancestor. The same information is available from `volume_field::tag_usage()`,
`untagged_volumes()` and `magnetised_volumes()`.

**Discontinuities.** The field is piecewise constant and jumps at volume boundaries (magnet volume to coil and yoke,
0.6 T to −1.75 T). Inside `MagIntVol_volume` it is uniform.

### Probes and threads

`geoinfo` is a single object shared by all modules, so it keeps no lookup state:

- `geoinfo::field_at` locates the point from the top of the geometry (`TGeoNavigator::FindNode`) with the shared
  navigator of `root_tgeomanager::navigator()`, and leaves that navigator at the point, as `get_info()` does. It is
  correct whatever other code did with the navigator before.
- A `geoinfo::field_probe` holds its own state: a TGeoNavigator of its own, not registered in the TGeoManager (so
  creating it does not change the current navigator), and the field of the last volume found. Each call checks
  whether the point is still in the last volume (`TGeoNavigator::IsSameLocation`), otherwise relocates its navigator
  from the last location, and reads the volume from the navigator state. The result is always the field of the
  volume containing the point; other code moving the shared navigator does not affect it, and the probe does not move
  the shared navigator. For a uniform field a probe just returns the value.

Threads: `field_at` must not be called from several threads at once (shared navigator). A probe is changed by every
call, so one probe must not be used from several threads at once. Probes used concurrently from different threads are
not supported either: ROOT shares the navigation scratch data between threads unless the TGeoManager is in
multi-threaded mode (`TGeoManager::SetMaxThreads`), which sandreco does not enable. The field table is immutable after
construction.

Lifetime: a probe refers to the field of the `geoinfo` that made it and to the loaded geometry, and must not outlive
them. It is movable, not copyable; a default-constructed or moved-from probe is unbound and throws on use.

### Performance

Measured by the `geoinfo_field` test (Debug build, SAND_opt3_DRIFT1 geometry), on 50 straight lines through SAND:

| Points apart | `field_probe::field_at` | `geoinfo::field_at` |
|--------------|-------------------------|---------------------|
| 0.5 mm       | 0.12 μs                 | 0.21 μs             |
| 5 mm         | 0.19 μs                 | 0.26 μs             |
| 25 mm        | 0.32 μs                 | 0.35 μs             |

A probe is worth keeping for propagation-like access (nearby points), where it reuses the last volume; for scattered
points the two are similar. Creating a probe costs about 70 μs (navigator and cache), so make one per caller, not per
call. Constructing `geoinfo` with the GDML field (reading the GDML, 12–16 MB, and building the table) takes about
0.1 s at job start.

### Tests

- `tests/standalone/geoinfo/geoinfo_field`: parsing of BField values and GDML files; field on the
  `SAND_opt3_DRIFT1` geometry and GDML of the image data (tracker, calorimeter, yoke barrel and endcaps, outside SAND
  and the world), with `field_at` and a probe; probe against `field_at` on random points and lines; probe unaffected
  by, and not moving, the shared navigator; moved and unbound probes; uniform field; missing `"field"` key; relative
  GDML path; configuration errors; EField-only and daughter tags; time per lookup (printed with
  `--log_level=message`).
- `tests/standalone/geoinfo/geoinfo_field_synthetic`: inheritance on a small geometry built in the test, with a
  logical volume placed in two mothers: inheritance through all placements, own tags, conflicting fields, `V` and
  `V_PV` names, setup summary.
- `tests/framework/041_geoinfo_field.json`: a job with the GDML field through a relative path, checking points with
  `field_at` and with a probe kept across events.

The standalone tests read the sandreco data of the image from the CMake cache variable `SANDRECO_IMAGE_DATA_DIR`
(default `/usr/local/share/sandreco/data`), passed to every standalone test as `SAND_IMAGE_DATA_DIR`; it does not
depend on `CMAKE_INSTALL_PREFIX`.
