# Kalman filter library for the tracker

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field, the measurement models for a single
wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the measurement model
applies to drift chambers and straw tubes alike, and is the same model used by the fast digitizations
`drift_fast_digi` and `stt_fast_digi`.

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/particle.hpp>`,
`<kalman/propagator.hpp>`, `<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/field.hpp>` and
`<kalman/engine.hpp>`. The implementation is in `.cpp` files and depends on ROOT `SMatrix`, on the CLHEP units of the
Geant4 installation (header only), on ROOT's `TDatabasePDG` (for `mass_from_pdg`) and on ufw (for the engine
configuration).

The math knows nothing about digits or about the geometry service. The reconstruction modules read the digits and the
wire descriptions, build the inputs described below and call the library. A change in the digit format or in the
geometry only affects the modules.

The formulas below are written in plain text so that they read the same in GitLab and in Doxygen: `a . b` is the
scalar product, `a x b` the vector product, `|a|` the norm, `M^T` the transpose. The Doxygen documentation of each
function points to its section here.

## Public interface and namespace detail

Everything in `sand::tracker::kalman` is the public interface, used by the reconstruction modules.

The nested namespace `sand::tracker::kalman::detail` holds helpers that are not part of that interface:

- `detail::derivatives()`: right-hand side of the equations of motion and its Jacobian, used by `propagate()`;
- `detail::wire_distance()`: distance of closest approach and position along the wire, used by `wire_time()`.

Modules should not call them. They are declared in the headers, rather than hidden in the `.cpp` files, only because
the standalone tests check them directly against analytic results and finite differences.

## Configuration: no defaults in the code

Every tunable parameter comes from the configuration; the code has no default values. The structs that carry the
parameters (`reco_calibration`, `propagator_config`) have no default member values, `propagate()` has no default
settings argument (it throws if `max_step` or `max_slope` is not positive, as in a value-initialised
`propagator_config`), and every key of the engine configuration is required: a missing key is an error that names
it. Code that builds the structs directly (tests, standalone tools) sets every member.

Physical constants are not configuration and are not written as numbers either: they come from CLHEP (see section
"Track state") and masses from the particle table (`mass_from_pdg`).

The only fixed number in the library is `s_wire_plane_tolerance` (see section "Wire measurements"), a geometric
sanity check of the wire description, not a physics or configuration value.

## Track state

Header `state.hpp`.

### Units, constants and the field convention

| Quantity | Unit |
|----------|------|
| length   | mm   |
| time     | ns   |
| momentum, mass, energy | MeV |
| magnetic field | T (a plain number) |
| charge | e |

Lengths, times and energies are the CLHEP internal units, so CLHEP values can be used directly. The constants come
from `<Geant4/CLHEP/Units/PhysicalConstants.h>` and `<Geant4/CLHEP/Units/SystemOfUnits.h>`, with qualified names
(`CLHEP::c_light`, `CLHEP::tesla`, ...); the `G4*.hh` wrappers are not used, because they put short names such as
`m`, `s`, `g` in the global namespace. The speed of light is `CLHEP::c_light` = 299.792458 mm/ns.

The magnetic field is a number in tesla everywhere in the library: in the `field` interface, in the engine
configuration and in the values read from the GDML. It is not converted to the CLHEP internal unit (where
`CLHEP::tesla` = 0.001). The curvature constant `kappa` is therefore derived, not written:

```
kappa = CLHEP::c_light * CLHEP::tesla = 0.299792458   [MeV / (mm T)]

p [MeV] = kappa * |q| * B [T] * R [mm]        (q in units of e)
```

Masses are an argument of the library (the mass hypothesis). Code that needs the mass of a species calls
`mass_from_pdg(pdg)` (`particle.hpp`), which reads ROOT's `TDatabasePDG` (in GeV, converted with `CLHEP::GeV`), caches
the value and throws `std::invalid_argument` for an unknown code.

### Parameters

The state is defined on a plane of constant `z`:

| Index | Parameter | Meaning |
|-------|-----------|---------|
| `X`, `Y` | x, y | position on the plane [mm] |
| `TX`, `TY` | tx, ty | slopes dx/dz, dy/dz |
| `QOP` | q/p | charge over momentum [1/MeV] |
| `T` | t | time at which the particle crosses the plane [ns] |

```
x = (x, y, tx, ty, q/p, t)        at the plane z
```

The time is part of the state, so the fit determines the track time together with the trajectory. This is needed
inside the 10 μs spill, where the time of each track is not known in advance. A straight track has q/p = 0.

The covariance `C` is the symmetric 6 x 6 matrix of the same parameters, in the same order and units
(`sym_matrix_t`).

### Direction and z as the independent variable

The independent variable is `z`: the trajectory is described by its crossings of planes at fixed z, which is where
the tracker measures it. Consequences:

- the state also carries `direction` = d = sign(p_z), +1 for a particle moving towards larger z and -1 towards smaller
  z. The slopes keep their meaning as dx/dz and dy/dz in both cases; the direction of motion is
  `d * (tx, ty, 1) / sqrt(1 + tx^2 + ty^2)`;
- a track parallel to the planes (p_z = 0) cannot be represented: its slopes are infinite. The propagation stops
  when |tx| or |ty| exceed `propagator_config::max_slope`, which happens for tracks that curl back in the field;
- the direction is a property of the particle, not of the propagation: propagating a state to a plane behind it
  runs the equations backwards and keeps `direction`.

## Magnetic field

Header `field.hpp`. `field` is an interface returning the field (Bx, By, Bz) in T at a position in mm, in the global
frame. `uniform_field` is the only implementation for now. The map read from the BField auxiliary tags of the GDML
will be a second implementation of the same interface.

## Propagation

Header `propagator.hpp`. `propagate(state, z_target, mass, field, config)` moves a state to another plane with a
fourth-order Runge-Kutta in z and transports its covariance.

### Equations of motion

For a particle of charge q (in units of e) and momentum p in a static magnetic field, with s the path length and
`T = dr/ds` the unit tangent, the Lorentz force gives

```
d^2 r / ds^2 = dT/ds = kappa * (q/p) * (T x B)
```

(B in T, lengths in mm, p in MeV). With z as independent variable, d = sign(p_z) and N = sqrt(1 + tx^2 + ty^2):

```
dr/dz = (tx, ty, 1),    T = d * (tx, ty, 1) / N,    ds/dz = 1 / T_z = d * N
```

Since tx = T_x / T_z,

```
dtx/dz = (ds/dz) * d(T_x / T_z)/ds = d * N * (T_x' T_z - T_x T_z') / T_z^2       (' = d/ds)
```

and with T' = kappa (q/p) (T x B), T x B = (T_y B_z - T_z B_y, T_z B_x - T_x B_z, T_x B_y - T_y B_x), dividing by
T_z^2 leaves only the slopes:

```
dtx/dz  = d * kappa * (q/p) * N * ( tx*ty*Bx - (1 + tx^2)*By + ty*Bz )
dty/dz  = d * kappa * (q/p) * N * ( (1 + ty^2)*Bx - tx*ty*By - tx*Bz )
```

The other components:

```
dx/dz   = tx
dy/dz   = ty
dqop/dz = 0                                     (no energy loss in the propagator)
dt/dz   = (ds/dz) / (beta c) = d * N * sqrt(1 + m^2 (q/p)^2) / c
```

where 1/beta = E/p = sqrt(1 + m^2/p^2) = sqrt(1 + m^2 (q/p)^2) for a unit charge, m is the mass hypothesis and
c = `CLHEP::c_light`. The mass enters only the time of flight.

Approximations: the charge is ±1 in the time of flight; energy loss and multiple scattering are not included (they
belong to a material model between planes).

### Runge-Kutta integration

With the state vector s and the right-hand side f(s, z) above, each step of length h (|h| <= `max_step`, the
interval being divided into equal steps) is the classical fourth-order Runge-Kutta:

```
k1 = f(s, z)
k2 = f(s + h/2 k1, z + h/2)
k3 = f(s + h/2 k2, z + h/2)
k4 = f(s + h k3,   z + h)
s(z + h) = s + h/6 (k1 + 2 k2 + 2 k3 + k4)
```

The field is evaluated at the position of stages 1, 2 and 4; stage 3 reuses the field of stage 2 (the two midpoint
positions differ only by the change of slope over half a step), which saves one field query per step and is exact
for a uniform field. A negative h propagates towards smaller z.

### Transport Jacobian

The Jacobian `F = d s(z1) / d s(z0)` satisfies

```
dF/dz = A(s(z), z) F,    F(z0) = I,    A = df/ds
```

and is integrated with the same Runge-Kutta stages, using the A of each stage:

```
K1 = A1 F
K2 = A2 (F + h/2 K1)
K3 = A3 (F + h/2 K2)
K4 = A4 (F + h K3)
F(z + h) = F + h/6 (K1 + 2 K2 + 2 K3 + K4)
```

`A` is computed analytically by `detail::derivatives()`. The derivatives of the field with respect to the position
are neglected (the field is treated as constant within each stage), which is exact for a uniform field. The
covariance is transported as

```
C(z1) = F C(z0) F^T
```

### Path length

The path length is accumulated with the trapezoidal rule on ds/dz over each step:

```
path_length += |h| * (N(z) + N(z + h)) / 2
```

It is used by the material model and is exact for a straight track.

## Wire measurements

Header `measurement.hpp`.

### Wire geometry

A wire is described by `wire_geometry`, built from its two ends in the global frame: `head` is the readout end and
`tail` the far end. The models assume that the wire lies in a plane of constant z, so the constructor checks it: it
throws `std::invalid_argument` if `|head.z - tail.z|` exceeds `s_wire_plane_tolerance` = 1 μm
(`1 * CLHEP::micrometer`), or if the wire has zero length. The tolerance is a geometric sanity check (the geometry
describes wires in planes of constant z, up to rounding), not a physics or configuration value. Any number of views
and any stereo angle are handled in the same way.

Both models need the state on the wire plane (`state.z == wire.z()`) and return the predicted value with its
derivative row `H` = d(value)/d(state).

### Notation

```
P0 = (x, y, z)            track point on the wire plane
v  = (tx, ty, 1)          track direction (not normalised); near the plane the track is P(lambda) = P0 + lambda v
H  = head                 readout end of the wire
u  = (ux, uy, 0)          unit vector along the wire, from head to tail; the wire is W(mu) = H + mu u
n  = (-uy, ux)            in-plane unit normal to the wire
Delta = P0 - H            zero z component: track point and wire are on the same plane

t_u = u . (tx, ty)        slope along the wire
t_n = n . (tx, ty)        slope across the wire
```

### wire_position

The signed coordinate of the track across the wire, on the wire plane:

```
value = n . Delta,    H = (nx, ny, 0, 0, 0, 0)
```

Used in track finding, with the wire position as measurement (value 0) and a flat error (pitch^2 / 12 or
max_radius^2 / 3).

### Distance of closest approach d and position along the wire L

`detail::wire_distance()`. The signed distance of closest approach between the track line and the wire line is the
projection of Delta on their common normal v x u:

```
d = Delta . (v x u) / |v x u|
```

With v x u = (-uy, ux, uy tx - ux ty) and Delta_z = 0, the numerator is n . Delta; the norm is

```
|v x u|^2 = uy^2 + ux^2 + (uy tx - ux ty)^2 = 1 + t_n^2
```

so that

```
d = n . Delta / sqrt(1 + t_n^2)
```

d is positive when the track passes on the side of n. For a track perpendicular to the planes it is the distance on
the plane; a slope across the wire shortens it by 1 / sqrt(1 + t_n^2).

L is the distance from the readout end of the point of the wire closest to the track: the value of mu that minimises
`|P0 + lambda v - H - mu u|^2` over lambda and mu. Setting both derivatives to zero,

```
lambda (v.v) - mu (u.v) = -Delta.v
lambda (u.v) - mu       = -Delta.u          (u.u = 1)
```

and eliminating lambda:

```
L = (Delta.u - (Delta.v)(u.v)/(v.v)) / (1 - (u.v)^2/(v.v))
```

Since v.v = 1 + t_u^2 + t_n^2, u.v = t_u and Delta.v = (Delta.u) t_u + (n.Delta) t_n, this is also

```
L = Delta.u - (n.Delta) t_n t_u / (1 + t_n^2)
```

the form used for the derivatives. The denominator 1 + t_n^2 is at least 1.

### wire_time

The TDC time is predicted as

```
t_TDC = t + L / v_wire + side * d / v_drift + t_offset
```

with t the track time on the wire plane, v_wire the signal speed along the wire, v_drift the drift velocity and
t_offset a fixed electronics offset (all from `reco_calibration`). Used in the final fit.

**Why side * d.** The drift time is |d| / v_drift: the electrons drift from the closest approach point to the wire
whichever side the track passes. But |d| is not differentiable at d = 0, and the Kalman update needs the derivative.
The model therefore uses side * d, with the left-right hypothesis `side` (+1 or -1) fixed during the update:

- with the right hypothesis (side = sign of d) the term equals |d| / v_drift: the drift time is never subtracted;
- with the wrong hypothesis it predicts a negative drift time; the residual is large, and the deterministic annealing
  filter, which treats the two sides as two competing measurements, gives that hypothesis a low weight.

The model is smooth across the wire for each hypothesis, which is what the annealing needs.

**Derivatives.** H is computed analytically from the side-fixed model. With a = u . Delta, b = n . Delta and
S2 = 1 + t_n^2:

```
d = b / sqrt(S2)                     L = a - b t_n t_u / S2

dd/db  = 1 / sqrt(S2)                dL/da   = 1
dd/dtn = -b t_n / S2^(3/2)           dL/db   = -t_n t_u / S2
                                     dL/dt_u = -b t_n / S2
                                     dL/dt_n = -b t_u (1 - t_n^2) / S2^2
```

x and y enter through a and b (da/dx = ux, db/dx = nx, da/dy = uy, db/dy = ny), tx and ty through t_u and t_n
(dt_u/dtx = ux, dt_n/dtx = nx, dt_u/dty = uy, dt_n/dty = ny), q/p does not enter:

```
dt_TDC/dx  = ux (dL/da)/v_wire   + nx [ (dL/db)/v_wire   + side (dd/db)/v_drift  ]
dt_TDC/dy  = uy (dL/da)/v_wire   + ny [ (dL/db)/v_wire   + side (dd/db)/v_drift  ]
dt_TDC/dtx = ux (dL/dt_u)/v_wire + nx [ (dL/dt_n)/v_wire + side (dd/dt_n)/v_drift ]
dt_TDC/dty = uy (dL/dt_u)/v_wire + ny [ (dL/dt_n)/v_wire + side (dd/dt_n)/v_drift ]
dt_TDC/d(q/p) = 0
dt_TDC/dt     = 1
```

Approximations: the track is a straight line near the wire plane (the curvature over a drift cell is negligible);
the drift is linear in the distance (constant drift velocity); the time of flight from the plane to the closest
approach point is neglected.

The parameters of `reco_calibration` (drift velocity, wire velocity, TDC resolution, time offset) are what the
reconstruction assumes. They are kept separate from the parameters of the digitization, even when they have the
same values.

## Kalman update

Header `filter.hpp`. `update(state, prediction, measured, variance, weight)` adds one scalar measurement m with
variance V to the predicted state (x-, C-) on the measurement plane. The prediction gives h(x-) and the row H.

```
r = m - h(x-)                         residual
S = H C- H^T + V / w                  variance of the residual
K = C- H^T / S                        gain (a 6-vector)
x = x- + K r
C = (I - K H) C- (I - K H)^T + K (V / w) K^T
```

The weight w in (0, 1] scales the measurement variance as V / w: a measurement with a small weight pulls the state
less. It is the hook for the deterministic annealing filter, where each wire side enters with its own weight; w = 1
is the ordinary Kalman update.

**Joseph form.** The covariance is updated in Joseph form rather than as `C = (I - K H) C-`. The two are equal for the
optimal gain, but the short form subtracts two nearly equal matrices when the measurement is precise, and under
rounding it can lose symmetry and positive definiteness. The Joseph form is a sum of two positive semidefinite terms,
so it stays positive semidefinite; it is also correct for any gain. The result is symmetrised explicitly.

**Outputs.**

```
chi2           = r^2 / (H C- H^T + V)       predicted chi2 increment, with the unweighted V
residual       = r
residual_after = r - (H K) r = r (V / w) / S
```

The chi2 increment uses the unweighted variance, so that it measures the compatibility of the measurement with the
prediction independently of the weight; for w = 1 it is r^2 / S. The residual after the update is the linearised
residual of the updated state: m - h(x) to first order in x - x-.

## Smoother

`smooth(steps)` is a Rauch-Tung-Striebel smoother over the steps of a forward filter pass. With k|k the filtered
state on plane k, k+1|k the state predicted on plane k+1 from plane k, F_{k+1} the transport Jacobian from plane k to
plane k+1 and n the last plane:

```
A_k   = C_k|k F_{k+1}^T (C_{k+1|k})^-1
x_k|n = x_k|k + A_k (x_{k+1|n} - x_{k+1|k})
C_k|n = C_k|k + A_k (C_{k+1|n} - C_{k+1|k}) A_k^T
```

starting from x_n|n, C_n|n (the last filtered state) and going backwards. Each `filter_step` stores what the smoother
needs at its plane: `predicted` (x_{k|k-1}, C_{k|k-1}), `filtered` (x_k|k, C_k|k) and `F` (the Jacobian from the
previous plane; unused for the first step). Since the predicted covariance is stored as it was used by the filter,
any process noise added in the prediction (material) is included automatically. The function returns nothing if a
predicted covariance cannot be inverted.

## Engine

Header `engine.hpp`. `sand::tracker::kalman::engine` is the complex data object of the library. The seeder, the
combinatorial track finder, the fit and smoother and the vertex fit all need the same state: magnetic field, material
description and reconstruction calibration. Keeping that state in one global, unique complex data object means it is
configured once per job and every module sees the same instance; and since the library is compiled once into
`sand_tracker_kalman`, every module runs the same code, instead of each one compiling its own copy of header-only
functions (which duplicates code and can give slightly different numerical results).

The engine is configured in the `"globals"` section of the job. Every key is required (see
section "Configuration: no defaults in the code"):

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
| `calibration.drift_velocity` | `double`    | mm/ns | Required         | Drift velocity assumed by the reconstruction.   |
| `calibration.wire_velocity`  | `double`    | mm/ns | Required         | Signal propagation speed along the wire.        |
| `calibration.tdc_resolution` | `double`    | ns    | Required         | TDC time resolution.                            |
| `calibration.t0_offset`      | `double`    | ns    | Required         | Fixed electronics offset.                       |
| `propagator.max_step`        | `double`    | mm    | Required         | Maximum Runge-Kutta step.                       |
| `propagator.max_slope`       | `double`    |       | Required         | Propagation fails if \|tx\| or \|ty\| exceed it.  |

Velocities, resolution, step and slope must be positive. A missing key is an error that names it
(`missing required key 'calibration.drift_velocity'`), and unknown keys are rejected, so a misspelt key is an error
and not a silent default.

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
double const m  = mass_from_pdg(13); // muon hypothesis

wire_geometry const w(head, tail);   // ends from the wire description of the geometry service
double tdc;                          // from the digit
int side;                            // left-right hypothesis

auto prop = kf.propagate(current, w.z(), m);
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

- `kalman_propagator`: helix in a uniform field against the analytic solution (radius p / (kappa B), time of flight
  with the muon mass from the table), round trip forward and back, Jacobian against finite differences in both
  directions, particle moving towards smaller z, rejection of unset propagation settings;
- `kalman_filter`: closest approach distance; analytic derivatives of the time model against Richardson-extrapolated
  central differences on 2000 random states and wires (straight and stereo, both sides, relative tolerance 1e-6 plus
  the rounding bound of the differences, which only matters for derivatives close to zero); checks of the
  `wire_geometry` constructor; `mass_from_pdg` against `TDatabasePDG`; pulls of the fitted parameters on toy tracks;
- `kalman_engine`: engine built from a JSON configuration, configured values, an error naming each missing key,
  rejection of invalid configurations, `engine.propagate` and `engine.wire_time` identical to the free functions with
  the same field, settings and calibration.

The tests link `sand_tracker_kalman` and `ROOT::EG`.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun and needs an edep-sim file; build it against `sand_tracker_kalman` (include path `src/data/tracker` and the
Geant4 include directory for CLHEP), ROOT (with `libEG`) and the edep-sim I/O library.

## Validation so far

- Propagator against edep-sim truth (1 GeV μ⁻ in the tracker, 200 events): the mean deviation is a few μm with the
  field on (dy = -0.008 mm, rms 0.21 mm), against about 1 mm with the field off. The spread is multiple scattering,
  not yet modelled. The time of flight agrees within 0.1 ps. The same check on μ±, π± (0.5-2 GeV) and protons
  (100-750 MeV) gives the same results before and after the move to CLHEP constants and table masses.
- Toy fits: pull means within ±0.12 and widths between 0.98 and 1.10 for all parameters.
