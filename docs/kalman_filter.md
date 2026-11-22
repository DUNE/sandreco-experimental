# Kalman filter library for the tracker

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field, the measurement models for a single
wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the measurement model
applies to drift chambers and straw tubes alike, and is the same model used by the fast digitizations
`drift_fast_digi` and `stt_fast_digi`.

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/propagator.hpp>`,
`<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/field.hpp>` and `<kalman/engine.hpp>`. The
implementation is in `.cpp` files and depends on ROOT `SMatrix` and on ufw (for the engine configuration).

The math knows nothing about digits or about the geometry service. The reconstruction modules read the digits and the
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

`field` is an interface returning the field at a position. `uniform_field` is the only implementation for now. The map
read from the BField auxiliary tags of the GDML will be a second implementation of the same interface.

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
| `field`                      | `double[3]` | T     | Required         | Uniform magnetic field (Bx, By, Bz).            |
| `calibration.drift_velocity` | `double`    | mm/ns | 0.05             | Drift velocity assumed by the reconstruction.   |
| `calibration.wire_velocity`  | `double`    | mm/ns | 200              | Signal propagation speed along the wire.        |
| `calibration.tdc_resolution` | `double`    | ns    | 3.5              | TDC time resolution.                            |
| `calibration.t0_offset`      | `double`    | ns    | 0                | Fixed electronics offset.                       |
| `propagator.max_step`        | `double`    | mm    | 50               | Maximum Runge-Kutta step.                       |
| `propagator.max_slope`       | `double`    |       | 1000             | Propagation fails if \|tx\| or \|ty\| exceed it.  |

`calibration` and `propagator` are optional, and so is each of their keys; the defaults are those of
`reco_calibration` and `propagator_config`. Velocities, resolution, step and slope must be positive. Unknown keys are
rejected, so a misspelt key is an error and not a silent default.

The field is uniform for now. It is owned by the engine through a pointer to the `field` interface, so the field map
read from the GDML will replace it without changing the modules.

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
  settings and calibration.

The tests link `sand_tracker_kalman`.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun and needs an edep-sim file; build it against `sand_tracker_kalman` (include path `src/data/tracker`) and the
edep-sim I/O library.

## Validation so far

- Propagator against edep-sim truth (1 GeV μ⁻ in the tracker): the mean deviation is a few μm with the field on,
  against about 1 mm with the field off. The spread, about 0.2 mm, is multiple scattering, not yet modelled. The
  time of flight agrees within 0.1 ps.
- Toy fits: pull means within ±0.12 and widths between 0.99 and 1.09 for all parameters.
