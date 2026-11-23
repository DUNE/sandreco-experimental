# Kalman filter library for the tracker

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field, the measurement models for a single
wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the measurement model
applies to drift chambers and straw tubes alike, and is the same model used by the fast digitizations
`drift_fast_digi` and `stt_fast_digi`.

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/propagator.hpp>`,
`<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/field.hpp>`, `<kalman/geometry_field.hpp>` and
`<kalman/engine.hpp>`. The implementation is in `.cpp` files and depends on ROOT `SMatrix`, on ufw (for the engine
configuration) and, for the field of the geometry volumes, on `sand_root_tgeomanager` and ROOT's geometry and XML
libraries.

The math knows nothing about digits or about the geometry service (the field of the geometry volumes uses only the
TGeoManager of `root_tgeomanager`). The reconstruction modules read the digits and the
wire descriptions, build the inputs described below and call the library. A change in the digit format or in the
geometry only affects the modules.

## Conventions

| Quantity | Unit |
|----------|------|
| length   | mm   |
| time     | ns   |
| momentum, mass, energy | MeV |
| magnetic field | T |

Charges are in units of e. The constants are `s_c = 299.792458` mm/ns and `s_kappa = 0.299792458`, so that
`p = s_kappa * B * R` for a unit charge.

## Track state (`state.hpp`)

The state is defined on a plane of constant `z`:

| Index | Parameter | Meaning |
|-------|-----------|---------|
| `X`, `Y` | x, y | position on the plane [mm] |
| `TX`, `TY` | tx, ty | slopes dx/dz, dy/dz |
| `QOP` | q/p | charge over momentum [1/MeV] |
| `T` | t | time at which the particle crosses the plane [ns] |

The time is part of the state, so the fit determines the track time together with the trajectory. This is needed
inside the 10 μs spill, where the time of each track is not known in advance.

Since `z` is the independent variable, the state also carries `direction`, the sign of p_z. A particle moving towards
smaller z has `direction = -1`; the slopes keep their meaning as dx/dz and dy/dz.

## Magnetic field (`field.hpp`)

`field` is an interface returning the field (T) at a position (mm, global frame). There are two implementations:

- `uniform_field` (`field.hpp`): the same value everywhere; useful for tests and as a first approximation inside the
  magnet;
- `volume_field` (`geometry_field.hpp`): the field of the volume containing the point, from the BField auxiliary tags
  of the GDML.

### Field of the geometry volumes (`geometry_field.hpp`)

**Source.** The SAND GDML defines the field with auxiliary tags on volumes, in the format read by edep-sim:

```xml
<volume name="MagIntVol_volume">
  <auxiliary auxtype="BField" auxvalue="(0.600000 T, 0.0 T, 0.0 T)"/>
```

In `nd_hall_with_lar_tms_sand_drift1_v2026.03.06.gdml` there are eight such tags: `MagIntVol_volume`, the volume
containing the calorimeter and the tracker, with 0.6 T along x; `KLOEYokeBarrel_volume` with −1.746067 T along x; six
TMS volumes with ±1 T along y. The yoke endcaps have no tag.

The geometry of a job is the TGeoManager loaded by `sand::root_tgeomanager`, usually from an edep-sim file. That
TGeoManager does not keep the auxiliaries, and importing the GDML again is not an option (ROOT allows one
TGeoManager per process, and a GDML import uses cm instead of the mm of the edep-sim geometry). So
`read_gdml_field_tags(path)` reads only the `<volume>` elements and their `BField`/`EField` auxiliaries, with ROOT's XML
parser, and `volume_field` matches them to the loaded volumes by name: GDML volume `V` is the TGeoVolume `V` or, as
edep-sim writes it, `V_PV`. A tag whose volume is not in the geometry is an error. The components are numbers with an
optional unit (`T`, `tesla`, `kG`, `kilogauss`, `G`, `gauss`; tesla when missing), with or without the surrounding
parentheses. External XML entities are not expanded.

**Inheritance.** The rule is the one of edep-sim, which attaches a field manager to each tagged logical volume and
forces it onto all its daughters, visiting the tree from the top:

- a volume has the field of its nearest tagged ancestor, itself included: the tracker, with no tag of its own, has
  the 0.6 T of `MagIntVol_volume`;
- a volume with an `EField` tag and no `BField` tag has B = 0 (edep-sim gives it a field manager of its own, with
  zero magnetic field);
- a volume without a tagged ancestor, and any point outside the world, has zero field;
- the field belongs to the logical volume, shared by all its placements. A volume that would inherit two different
  fields through different placements is an error, since in edep-sim the result would depend on the visiting order.
  In the SAND geometry every logical volume has a single mother, so this does not occur.

The table of the field of each logical volume is computed once, when the field is built, and summarised in the log
at info level: one line per tag with its GDML volume, the matching volume of the geometry, its field and the number
of logical volumes inheriting it (or "unused" if the volume is not placed in the tree), and the number of volumes with
no tagged ancestor. The same information is available from `tag_usage()` and `untagged_volumes()`.

**Performance.** A lookup calls `TGeoNavigator::IsSameLocation(x, y, z, true)` on the navigator of
`root_tgeomanager::navigator()`, which checks whether the point is still in the last volume and otherwise relocates
the navigator, searching from the current location; the field of the last volume is cached. This was the fastest of
the variants measured (always `FindNode`, `IsSameLocation` then `FindNode`), all with identical results. On lines
through SAND the time per call is about 0.1 μs with points 0.5 mm apart, 0.18 μs at 5 mm and 0.29 μs at 25 mm
(the RK4 calls the field four times per step). Reading the 16 MB GDML and building the table takes about 30 ms.

The navigator is the one shared by all users of `root_tgeomanager`: `at()` reads the volume from the navigator state at
every call, so it is right whatever other code did with the navigator in between, but other code must not rely on
the navigator position across a propagation.

**Threads.** `field::at` is const, but `volume_field` moves the navigator and updates its cache (mutable members).
One engine is not meant to be used from several threads at once.

**Discontinuities.** The field is piecewise constant and jumps at volume boundaries (magnet volume to coil and yoke,
0.6 T to −1.75 T). The transport Jacobian ignores field gradients, and a Runge-Kutta step crossing a boundary is less
accurate than one in a uniform field, since the stages sample the field on both sides. Inside `MagIntVol_volume` the
field is uniform, so tracker fits are not affected; propagations through the calorimeter, coil and yoke are.

## Propagation (`propagator.hpp`)

`propagate(state, z_target, mass, field, config)` moves a state to another plane with a fourth-order Runge-Kutta in z.
With `d = direction` and `N = sqrt(1 + tx^2 + ty^2)` the equations are

```
dx/dz   = tx
dy/dz   = ty
dtx/dz  = d * kappa * (q/p) * N * ( tx*ty*Bx - (1 + tx^2)*By + ty*Bz )
dty/dz  = d * kappa * (q/p) * N * ( (1 + ty^2)*Bx - tx*ty*By - tx*Bz )
dqop/dz = 0
dt/dz   = d * N * sqrt(1 + m^2 (q/p)^2) / c
```

The transport Jacobian `F` is integrated with the same Runge-Kutta steps, and the covariance is transported as
`F C F^T`. The target plane can be on either side of the starting one. The mass hypothesis enters only the time of
flight. Energy loss and multiple scattering are not included yet: they will be added by a material model between
planes.

The function returns nothing if the track becomes steeper than `propagator_config::max_slope`, which happens for
tracks that curl back in the field.

## Wire measurements (`measurement.hpp`)

A wire is described by `wire_geometry`, its two ends in the global frame: `head` is the readout end and `tail` the far
end. Any number of views and any stereo angle are handled in the same way. Two models are provided; both need the
state on the wire plane and return the predicted value with its derivative row `H`.

- `wire_position(state, wire)`: the signed coordinate of the track across the wire. Used in track finding, with the
  wire position as measurement (value 0) and a flat error.
- `wire_time(state, wire, side, calibration)`: the TDC time, predicted as

  ```
  t_TDC = t + L / v_wire + side * d / v_drift + t_offset
  ```

  where `d` is the signed distance of closest approach between track and wire, `L` the distance of the closest point
  from the readout end, and `side` the left-right hypothesis (+1 or -1). Used in the final fit.

The parameters of `reco_calibration` (drift velocity, wire velocity, TDC resolution, time offset) are what the
reconstruction assumes. They are kept separate from the parameters of the digitization, even when they have the
same values.

## Update and smoother (`filter.hpp`)

`update(state, prediction, measured, variance, weight)` adds one scalar measurement. The covariance is updated in
Joseph form, which keeps it symmetric and positive definite. The `weight` scales the variance as `V / weight`; it is
the hook for a deterministic annealing filter, where the two wire sides compete with their own weights. The result
contains the residual before and after the update and the chi2 increment.

`smooth(steps)` is a Rauch-Tung-Striebel smoother over the steps of a forward pass. Each `filter_step` keeps the
predicted and filtered state on a plane and the Jacobian from the previous plane.

## Engine (`engine.hpp`)

`sand::tracker::kalman::engine` is the complex data object of the library. The seeder, the combinatorial track
finder, the fit and smoother and the vertex fit all need the same state: magnetic field, material description and
reconstruction calibration. Keeping that state in one global, unique complex data object means it is configured once
per job and every module sees the same instance; and since the library is compiled once into `sand_tracker_kalman`,
every module runs the same code, instead of each one compiling its own copy of header-only functions (which
duplicates code and can give slightly different numerical results).

The engine is configured in the `"globals"` section of the job:

```json
"globals" : {
  "sand::tracker::kalman::engine" : {
    "field" : [0.6, 0.0, 0.0],
    "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0,
                      "tdc_resolution" : 3.5, "t0_offset" : 0.0 },
    "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 }
  }
}
```

| Parameter Name               | Type        | Unit  | Required/Default | Description                                     |
|------------------------------|-------------|-------|------------------|-------------------------------------------------|
| `field`                      | see below   | T     | Required         | Uniform field or field of the volumes.          |
| `calibration.drift_velocity` | `double`    | mm/ns | 0.05             | Drift velocity assumed by the reconstruction.   |
| `calibration.wire_velocity`  | `double`    | mm/ns | 200              | Signal propagation speed along the wire.        |
| `calibration.tdc_resolution` | `double`    | ns    | 3.5              | TDC time resolution.                            |
| `calibration.t0_offset`      | `double`    | ns    | 0                | Fixed electronics offset.                       |
| `propagator.max_step`        | `double`    | mm    | 50               | Maximum Runge-Kutta step.                       |
| `propagator.max_slope`       | `double`    |       | 1000             | Propagation fails if \|tx\| or \|ty\| exceed it.  |

`calibration` and `propagator` are optional, and so is each of their keys; the defaults are those of
`reco_calibration` and `propagator_config`. Velocities, resolution, step and slope must be positive. Unknown keys are
rejected, so a misspelt key is an error and not a silent default.

`field` is either an array `[Bx, By, Bz]`, a uniform field, or an object for the field of the geometry volumes:

```json
"globals" : {
  "sand::root_tgeomanager" : { "geometry" : "test/SAND_opt3_DRIFT1.sand-events-in-sand_inner_volume.2.edep.root" },
  "sand::tracker::kalman::engine" : {
    "field" : { "source" : "gdml", "file" : "geometries/sand/SAND_opt3_DRIFT1.gdml" }
  }
}
```

| Parameter Name | Type     | Required | Description                                                                 |
|----------------|----------|----------|-----------------------------------------------------------------------------|
| `field.source` | `string` | Required | Must be `"gdml"`.                                                           |
| `field.file`   | `path`   | Required | GDML file with the BField tags; relative paths are relative to `ufw-basepath`. |

`file` is resolved with `ufw::config::path_at`, as the `geometry` of `root_tgeomanager`. The tags are applied to the
geometry of `sand::root_tgeomanager`, which must be configured in the same job; the GDML must be the one the
geometry was made from. Unknown keys and a `source` other than `"gdml"` are errors.

The field is owned by the engine through a pointer to the `field` interface, so modules do not depend on which field
is configured.

A process gets the engine with `instance<sand::tracker::kalman::engine>()` and links `sand_tracker_kalman` in its
CMakeLists. The engine exposes

- `field()`, `calibration()`, `propagator_settings()`: the shared state;
- `propagate(state, z_target, mass)`: `propagate` with the field and settings of the engine;
- `wire_time(state, wire, side)`: `wire_time` with the calibration of the engine.

The free functions stay available with explicit arguments (the engine calls them), for tests and for code that needs
a different field or calibration.

## Example

One step of a forward fit, as a reconstruction module would write it:

```cpp
using namespace sand::tracker::kalman;

auto const& kf  = instance<engine>();
auto const& cal = kf.calibration();

wire_geometry w;       // filled from the wire description of the geometry service
double tdc;            // from the digit
int side;              // left-right hypothesis

auto prop = kf.propagate(current, w.z(), 105.66);
if (!prop) {
  // track lost
}
auto const pred = kf.wire_time(prop->out, w, side);
auto const upd  = update(prop->out, pred, tdc, cal.tdc_resolution * cal.tdc_resolution);
steps.push_back({prop->out, upd.out, prop->F});
current = upd.out;
```

## Tests

The standalone tests are in `tests/standalone/kalman/`:

- `kalman_propagator`: helix in a uniform field against the analytic solution, round trip forward and back,
  Jacobian against finite differences in both directions, particle moving towards smaller z;
- `kalman_filter`: closest approach distance, derivatives of the time model against finite differences, pulls of
  the fitted parameters on toy tracks;
- `kalman_engine`: engine built from a JSON configuration, defaults and configured values, rejection of invalid
  configurations, `engine.propagate` and `engine.wire_time` identical to the free functions with the same field,
  settings and calibration;
- `kalman_geometry_field`: parsing of BField values and of GDML files; field of the volumes on the
  `SAND_opt3_DRIFT1` geometry and GDML (tracker, calorimeter, yoke barrel and endcaps, outside the detector and the
  world), EField-only and daughter tags, cache correctness when the point or the shared navigator moves, propagation
  in the tracker identical to the uniform field, time per lookup (printed), configuration errors of the engine;
- `kalman_geometry_field_synthetic`: inheritance on a small geometry built in the test, with a logical volume placed
  in two mothers: inheritance through all placements, own tags, conflicting fields, `V` and `V_PV` names.

The tests link `sand_tracker_kalman`. `kalman_geometry_field` reads the sandreco data of the image, the
`ufw-basepath` of the framework tests. The directory is the CMake cache variable `SANDRECO_IMAGE_DATA_DIR` (default
`/usr/local/share/sandreco/data`), passed to every standalone test as `SAND_IMAGE_DATA_DIR`; it does not depend on
`CMAKE_INSTALL_PREFIX`.

The framework test `500_kalman_engine_geometry_field` builds the engine in a job with
`"field" : { "source" : "gdml", ... }` and a relative GDML path, and checks the field at points in the tracker, the
calorimeter, the yoke and outside, and a propagation in the tracker against the uniform field.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun and needs an edep-sim file; build it against `sand_tracker_kalman` and `sand_root_tgeomanager` (include paths
`src/data/tracker` and `src/data/common`) and the edep-sim I/O library. Usage:

```
gun_propagation_check <edep-sim file> [Bx in T | GDML file] [max events] [tracker | all]
```

With a GDML file the field is `volume_field` on the geometry of the edep-sim file. The region `tracker` (default)
takes pairs of consecutive true points inside a cylinder around the tracker; `all` takes every pair of the primary,
through the calorimeter, coil and yoke.

## Validation so far

- Propagator against edep-sim truth (1 GeV μ⁻ in the tracker): the mean deviation is a few μm with the field on,
  against about 1 mm with the field off. The spread, about 0.2 mm, is multiple scattering, not yet modelled. The
  time of flight agrees within 0.1 ps.
- Field of the geometry volumes, on particle-gun samples simulated with edep-sim from
  `nd_hall_with_lar_tms_sand_drift1_v2026.03.06.gdml` (μ±, π± at 0.5, 1, 1.5, 2 GeV; protons at 100, 200, 500,
  750 MeV), with the matching GDML. Position deviation in the bending coordinate y with the momentum of the start
  point, mean / RMS in mm:

  | Sample       | Pairs, tracker | Tracker, both fields | Pairs, all | All, uniform 0.6 T | All, field of the volumes |
  |--------------|----------------|----------------------|------------|--------------------|---------------------------|
  | μ⁻ 0.5 GeV   | 12859          | −0.012 / 0.34        | 39564      | −2.30 / 15.2       | −0.098 / 3.53             |
  | μ⁻ 1 GeV     | 13231          | −0.006 / 0.22        | 49144      | −13.5 / 96.1       | +0.063 / 4.67             |
  | μ⁻ 1.5 GeV   | 13320          | −0.007 / 0.19        | 53363      | −9.10 / 60.5       | +0.038 / 3.43             |
  | μ⁻ 2 GeV     | 10635          | −0.003 / 0.19        | 52165      | −6.94 / 41.7       | +0.046 / 3.44             |
  | μ⁺ 0.5 GeV   | 12684          | +0.014 / 0.35        | 39720      | +2.35 / 14.5       | +0.155 / 6.68             |
  | μ⁺ 1 GeV     | 13250          | +0.010 / 0.27        | 48958      | +14.1 / 97.9       | −0.047 / 4.24             |
  | μ⁺ 1.5 GeV   | 12663          | +0.004 / 0.20        | 52331      | +9.55 / 63.5       | −0.044 / 3.43             |
  | μ⁺ 2 GeV     | 11581          | −0.001 / 0.18        | 53100      | +6.72 / 40.2       | −0.003 / 2.96             |
  | π⁻ 0.5 GeV   | 10285          | −0.022 / 1.13        | 17265      | −0.57 / 9.90       | −0.049 / 2.48             |
  | π⁻ 1 GeV     | 8807           | −0.003 / 0.23        | 14391      | −0.85 / 18.6       | −0.020 / 2.10             |
  | π⁻ 1.5 GeV   | 6302           | −0.011 / 0.27        | 10425      | −1.22 / 23.4       | +0.008 / 2.89             |
  | π⁻ 2 GeV     | 5212           | −0.006 / 0.35        | 8859       | −1.17 / 19.1       | −0.019 / 2.10             |
  | π⁺ 0.5 GeV   | 10558          | −0.006 / 1.60        | 18368      | +0.77 / 9.16       | +0.062 / 2.92             |
  | π⁺ 1 GeV     | 8637           | −0.004 / 1.07        | 13998      | +1.26 / 31.3       | +0.044 / 1.91             |
  | π⁺ 1.5 GeV   | 6305           | +0.001 / 0.41        | 10281      | +1.09 / 22.5       | +0.044 / 2.28             |
  | π⁺ 2 GeV     | 5245           | +0.002 / 0.26        | 8772       | +0.97 / 17.0       | +0.034 / 2.00             |
  | p 100 MeV    | 6459           | +0.043 / 1.02        | 5459       | +0.042 / 0.82      | +0.042 / 0.82             |
  | p 200 MeV    | 12060          | +0.031 / 0.78        | 13551      | +0.037 / 0.86      | +0.037 / 0.86             |
  | p 500 MeV    | 9463           | +0.016 / 0.36        | 16444      | +0.088 / 1.38      | +0.039 / 1.21             |
  | p 750 MeV    | 8333           | +0.010 / 0.26        | 13434      | +0.26 / 2.16       | +0.042 / 1.31             |

  In the tracker the two fields give identical results for every sample, in x as well as in y. Over the
  whole trajectory the field of the volumes removes the bias of the yoke; the remaining spread comes from the material
  of the calorimeter and the yoke, not yet modelled, and from steps across field boundaries. The low-energy protons
  stop inside the magnet, so the two fields agree also on all points; with `all`, pairs whose end point has p_z ≈ 0
  are skipped, hence fewer pairs than in the tracker at 100 MeV. Samples dominated by effects other than the field:
  protons at 100 and 200 MeV by energy loss (at 100 MeV, 46% of the pairs lose more than 10% of the momentum between
  the two points, and they make most of the deviations above 3 mm); π± at 0.5 GeV and π⁺ at 1 GeV by a few large
  deviations without momentum loss, absent for muons of the same momentum, most likely hadronic elastic scattering.
- Toy fits: pull means within ±0.12 and widths between 0.99 and 1.09 for all parameters.
