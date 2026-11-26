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

## Notation

Each symbol has one meaning in the whole document and in the Doxygen documentation. Subscripts are written with
braces when they are more than one character (`x_{k|k}`); a subscript makes a different symbol (`v` and `v_d`).

| Symbol | Meaning | Unit |
|--------|---------|------|
| x, y, z | global coordinates; z is also the independent variable and labels the planes | mm |
| tx, ty | slopes dx/dz, dy/dz | |
| q | charge, in units of e | e |
| p, p_{perp} | momentum; its component perpendicular to the field | MeV |
| t | time at which the track crosses a plane | ns |
| m | mass hypothesis | MeV |
| beta, E | velocity over c; energy | MeV (E) |
| c, kappa | `CLHEP::c_light`; curvature constant `CLHEP::c_light * CLHEP::tesla` | mm/ns, MeV/(mm T) |
| B = (Bx, By, Bz) | magnetic field | T |
| sigma | sign of p_z: the `direction` of the state | |
| x(z), x_{k\|j} | state vector (x, y, tx, ty, q/p, t) at z; on plane k, using the measurements up to plane j | |
| C, C_{k\|j} | covariance of the state vector | |
| k, N | plane index; index of the last plane | |
| v, \|v\| | track direction (tx, ty, 1), not normalised; its norm sqrt(1 + tx^2 + ty^2) | |
| tau | unit tangent to the track, sigma v / \|v\| | |
| s | path length along the track | mm |
| P, P0 | point on the track; track point on the wire plane | mm |
| lambda, mu | parameters along the local track line and along the wire | mm |
| f, A | right-hand side dx(z)/dz of the equations of motion; its Jacobian df/dx | |
| h | Runge-Kutta step | mm |
| f_i, A_i, B_i, J_i | f, A and field at Runge-Kutta stage i; stage i of the Jacobian | |
| F, F_{k+1} | transport Jacobian; from plane k to plane k + 1 | |
| R | radius of the helix | mm |
| W0, W(mu) | readout end (head) of a wire; point on the wire, W0 + mu u | mm |
| u = (ux, uy, 0) | unit vector along the wire, from W0 to the far end | |
| n = (-uy, ux) | in-plane unit normal to the wire | |
| Delta | P0 - W0 | mm |
| t_u, t_n | slopes along and across the wire | |
| a, b | u . Delta, n . Delta | mm |
| rho | sqrt(1 + t_n^2) | |
| d | signed distance of closest approach between track and wire | mm |
| L | distance from W0 of the point of the wire closest to the track | mm |
| side | left-right hypothesis, +1 or -1 | |
| v_d, v_w | drift velocity; signal speed along the wire | mm/ns |
| t0_offset | fixed electronics offset | ns |
| t_{TDC} | TDC time | ns |
| sigma_t, sigma_{TDC}, sigma_x | total time resolution (`time_resolution`); TDC resolution; intrinsic spatial resolution | ns, ns, mm |
| measured, predicted | measured value; value predicted by a measurement model | |
| H | derivative row d(predicted)/dx of a measurement model | |
| V, w | measurement variance; weight of the measurement | |
| r, S, K | residual; its variance; Kalman gain | |
| I | identity matrix | |
| G_k | smoother gain on plane k | |

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

The rule applies to configuration parameters, not to function arguments: the weight argument of `update()` defaults to
1, a normal measurement (see section "Kalman update").

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
inside the 10 μs spill, where the time of each track is not known in advance. A straight track has q/p = 0 (see
section "Propagation" for its time of flight).

The covariance `C` is the symmetric 6 x 6 matrix of the same parameters, in the same order and units
(`sym_matrix_t`).

### Direction and z as the independent variable

The independent variable is `z`: the trajectory is described by its crossings of planes at fixed z, which is where
the tracker measures it. Consequences:

- the state also carries `direction` = sigma = sign(p_z), +1 for a particle moving towards larger z and -1 towards
  smaller z. The slopes keep their meaning as dx/dz and dy/dz in both cases; the direction of motion is
  tau = sigma v / |v|, with v = (tx, ty, 1);
- a track parallel to the planes (p_z = 0) cannot be represented: its slopes are infinite. The propagation stops
  when |tx| or |ty| exceed `propagator_config::max_slope`, which happens for tracks that curl back in the field;
- the direction is a property of the particle, not of the propagation: propagating a state to a plane behind it
  runs the equations backwards and keeps `direction`.

## Magnetic field

Header `field.hpp`. `field` is an interface returning the field (Bx, By, Bz) in T at a position in mm, in the global
frame. `uniform_field` is the only implementation for now. The map read from the BField auxiliary tags of the GDML
will be a second implementation of the same interface. The propagator reads the field at every Runge-Kutta stage.

## Propagation

Header `propagator.hpp`. `propagate(state, z_target, mass, field, config)` moves a state to another plane with a
fourth-order Runge-Kutta in z and transports its covariance.

### Equations of motion

For a particle of charge q (in units of e) and momentum p in a static magnetic field, with s the path length, P the
position and `tau = dP/ds` the unit tangent, the Lorentz force gives

```
d^2 P / ds^2 = dtau/ds = kappa * (q/p) * (tau x B)
```

(B in T, lengths in mm, p in MeV). With z as independent variable, sigma = sign(p_z) and v = (tx, ty, 1):

```
dP/dz = v,    tau = sigma * v / |v|,    ds/dz = 1 / tau_z = sigma * |v|,    |v| = sqrt(1 + tx^2 + ty^2)
```

Since tx = tau_x / tau_z,

```
dtx/dz = (ds/dz) * d(tau_x / tau_z)/ds = sigma * |v| * (tau_x' tau_z - tau_x tau_z') / tau_z^2       (' = d/ds)
```

and with tau' = kappa (q/p) (tau x B),
tau x B = (tau_y B_z - tau_z B_y, tau_z B_x - tau_x B_z, tau_x B_y - tau_y B_x), dividing by tau_z^2 leaves only the
slopes:

```
dtx/dz  = sigma * kappa * (q/p) * |v| * ( tx*ty*Bx - (1 + tx^2)*By + ty*Bz )
dty/dz  = sigma * kappa * (q/p) * |v| * ( (1 + ty^2)*Bx - tx*ty*By - tx*Bz )
```

The other components:

```
dx/dz   = tx
dy/dz   = ty
dqop/dz = 0                                         (no energy loss in the propagator)
dt/dz   = (ds/dz) / (beta c) = sigma * |v| * sqrt(1 + m^2 (q/p)^2) / c
```

where 1/beta = E/p = sqrt(1 + m^2/p^2) = sqrt(1 + m^2 (q/p)^2) for a unit charge and m is the mass hypothesis. The
mass enters only the time of flight.

**Straight tracks.** For q/p = 0 the time of flight uses 1/beta = sqrt(1 + m^2 * 0) = 1, that is beta = 1, and the
mass hypothesis has no effect: dt/dz = sigma |v| / c.

Approximations: the charge is ±1 in the time of flight; energy loss and multiple scattering are not included: the
library has no material model yet.

### Runge-Kutta integration

With the state vector x(z) and the right-hand side f(x, z) above, each step of length h (|h| <= `max_step`, the
interval being divided into equal steps) is the classical fourth-order Runge-Kutta:

```
f_1 = f(x(z), z)
f_2 = f(x(z) + h/2 f_1, z + h/2)
f_3 = f(x(z) + h/2 f_2, z + h/2)
f_4 = f(x(z) + h f_3,   z + h)
x(z + h) = x(z) + h/6 (f_1 + 2 f_2 + 2 f_3 + f_4)
```

The field B_i of each stage is read at the position (x, y) of the stage state and its z, so there are four field
queries per step. The two middle stages are at different positions (they differ by order h^2), so reading the field at
both is what makes the scheme a true fourth-order Runge-Kutta also in a non-uniform field: with the field of stage 2
reused for stage 3 the error would decrease only as h^2 (the test `fourth_order_in_non_uniform_field` checks the
order). For a uniform field the two are identical. A negative h propagates towards smaller z.

### Transport Jacobian

The Jacobian `F = d x(z1) / d x(z0)` satisfies

```
dF/dz = A(x(z), z) F,    F(z0) = I,    A = df/dx
```

and is integrated with the same Runge-Kutta stages, using the A_i of each stage (computed with the field B_i of that
stage):

```
J_1 = A_1 F
J_2 = A_2 (F + h/2 J_1)
J_3 = A_3 (F + h/2 J_2)
J_4 = A_4 (F + h J_3)
F(z + h) = F + h/6 (J_1 + 2 J_2 + 2 J_3 + J_4)
```

`A` is computed analytically by `detail::derivatives()`. The field is read at every stage, but its derivatives with
respect to the position (the field gradients) are still neglected in A, which is exact for a uniform field. The
covariance is transported as

```
C(z1) = F C(z0) F^T
```

### Path length

The path length is accumulated with the trapezoidal rule on ds/dz over each step:

```
path_length += |h| * (|v(z)| + |v(z + h)|) / 2
```

It is exact for a straight track. It is returned in `propagation_result::path_length`, but nothing in the library uses
it yet: it is meant for a material model (energy loss and multiple scattering), which will use it when it is added.

## Wire measurements

Header `measurement.hpp`.

### Wire geometry

A wire is described by `wire_geometry`, built from its two ends in the global frame: `head` is the readout end W0 and
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
W0 = head                 readout end of the wire
u  = (ux, uy, 0)          unit vector along the wire, from head to tail; the wire is W(mu) = W0 + mu u
n  = (-uy, ux)            in-plane unit normal to the wire
Delta = P0 - W0           zero z component: track point and wire are on the same plane

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
|v x u|^2 = uy^2 + ux^2 + (uy tx - ux ty)^2 = 1 + t_n^2 = rho^2
```

so that

```
d = n . Delta / rho,    rho = sqrt(1 + t_n^2)
```

d is positive when the track passes on the side of n. For a track perpendicular to the planes it is the distance on
the plane; a slope across the wire shortens it by a factor 1/sqrt(1 + t_n^2) = 1/rho.

L is the distance from the readout end of the point of the wire closest to the track: the value of mu that minimises
`|P0 + lambda v - W0 - mu u|^2` over lambda and mu. Setting both derivatives to zero,

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
L = Delta.u - (n.Delta) t_n t_u / rho^2
```

the form used for the derivatives. The denominator rho^2 = 1 + t_n^2 is at least 1.

### wire_time

The TDC time is predicted as

```
t_{TDC} = t + L / v_w + side * d / v_d + t0_offset
```

with t the track time on the wire plane, v_w the signal speed along the wire, v_d the drift velocity and t0_offset a
fixed electronics offset (all from `reco_calibration`). Used in the final fit.

**Why side * d.** The drift time is |d| / v_d: the electrons drift from the closest approach point to the wire
whichever side the track passes. But |d| is not differentiable at d = 0, and the Kalman update needs the derivative.
The model therefore uses side * d, with the left-right hypothesis `side` (+1 or -1) fixed during the update:

- with the right hypothesis (side = sign of d) the term equals |d| / v_d: the drift time is never subtracted;
- with the wrong hypothesis it predicts a negative drift time, and the residual is about 2 |d| / v_d. It is large only
  far from the wire, where the deterministic annealing filter, which treats the two sides as two competing
  measurements, gives the wrong hypothesis a low weight. Close to the wire, where 2 |d| / v_d is comparable with the
  time resolution sigma_t, the two hypotheses cannot be told apart; there the choice also matters little, since both
  predict nearly the same time.

The model is smooth across the wire for each hypothesis, which is what the annealing needs.

**Measurement variance.** The variance of the measured time in the update is sigma_t^2, where sigma_t is
`reco_calibration::time_resolution` (key `calibration.time_resolution`): the total resolution on the measured time,
the TDC resolution plus the intrinsic spatial resolution of the cell divided by the drift velocity,

```
sigma_t^2 = sigma_{TDC}^2 + (sigma_x / v_d)^2
```

The current fast digitizations (`drift_fast_digi`, `stt_fast_digi`) smear only the TDC time, with their `sigma_tdc`,
so with them sigma_t = sigma_{TDC}. A separate spatial term sigma_x will be added when the digitization models it.

**Derivatives.** H is computed analytically from the side-fixed model. With a = u . Delta, b = n . Delta and
rho = sqrt(1 + t_n^2):

```
d = b / rho                          L = a - b t_n t_u / rho^2

dd/db   = 1 / rho                    dL/da   = 1
dd/dt_n = -b t_n / rho^3             dL/db   = -t_n t_u / rho^2
                                     dL/dt_u = -b t_n / rho^2
                                     dL/dt_n = -b t_u (1 - t_n^2) / rho^4
```

x and y enter through a and b (da/dx = ux, db/dx = nx, da/dy = uy, db/dy = ny), tx and ty through t_u and t_n
(dt_u/dtx = ux, dt_n/dtx = nx, dt_u/dty = uy, dt_n/dty = ny), q/p does not enter:

```
dt_{TDC}/dx  = ux (dL/da)/v_w   + nx [ (dL/db)/v_w   + side (dd/db)/v_d   ]
dt_{TDC}/dy  = uy (dL/da)/v_w   + ny [ (dL/db)/v_w   + side (dd/db)/v_d   ]
dt_{TDC}/dtx = ux (dL/dt_u)/v_w + nx [ (dL/dt_n)/v_w + side (dd/dt_n)/v_d ]
dt_{TDC}/dty = uy (dL/dt_u)/v_w + ny [ (dL/dt_n)/v_w + side (dd/dt_n)/v_d ]
dt_{TDC}/d(q/p) = 0
dt_{TDC}/dt     = 1
```

Approximations: the track is a straight line near the wire plane (the curvature over a drift cell is negligible);
the drift is linear in the distance (constant drift velocity); the time of flight from the plane to the closest
approach point is neglected.

The parameters of `reco_calibration` (drift velocity, wire velocity, time resolution, time offset) are what the
reconstruction assumes. They are kept separate from the parameters of the digitization, even when they have the
same values.

## Kalman update

Header `filter.hpp`. `update(state, prediction, measured, variance, weight)` adds one scalar measurement with
variance V to the predicted state (x_{k|k-1}, C_{k|k-1}) on the measurement plane k. The prediction gives the
predicted value, predicted(x_{k|k-1}), and the row H.

```
r = measured - predicted(x_{k|k-1})               residual
S = H C_{k|k-1} H^T + V / w                       variance of the residual
K = C_{k|k-1} H^T / S                             gain (a 6-vector)
x_{k|k} = x_{k|k-1} + K r
C_{k|k} = (I - K H) C_{k|k-1} (I - K H)^T + K (V / w) K^T
```

The weight w in (0, 1] scales the measurement variance as V / w: a measurement with a small weight pulls the state
less. It is the hook for the deterministic annealing filter, where each wire side enters with its own weight; w = 1
is the ordinary Kalman update, and is the default value of the argument.

**Weight contract.** The weight must be positive. With w = 0 the variance V / w is infinite and the covariance becomes
NaN (0 times infinity in the Joseph form); a negative weight is a negative variance. `update()` therefore throws
`std::invalid_argument` for w <= 0 and for NaN, instead of returning a corrupted state. A caller that gives a
measurement no weight must skip the update: the deterministic annealing filter, which drops the hypotheses below its
minimum weight, never calls `update()` with w = 0 (it is not part of the library yet; this is the contract it will
follow).

**Joseph form.** The covariance is updated in Joseph form rather than as `C_{k|k} = (I - K H) C_{k|k-1}`. The two are
equal for the optimal gain, but the short form subtracts two nearly equal matrices when the measurement is precise,
and under rounding it can lose symmetry and positive definiteness. The Joseph form is a sum of two positive
semidefinite terms: in exact arithmetic it is positive semidefinite for any gain, and in floating point it is much
more robust under rounding than the short form, although not immune. The result is also symmetrised explicitly, as
(C + C^T) / 2, before it is stored in the symmetric matrix.

**Outputs.**

```
chi2           = r^2 / (H C_{k|k-1} H^T + V)      predicted chi2 increment, with the unweighted V
residual       = r
residual_after = r - (H K) r = r (V / w) / S
```

The chi2 increment uses the unweighted variance, so that it measures the compatibility of the measurement with the
prediction independently of the weight; for w = 1 it is r^2 / S. The residual after the update is the linearised
residual of the updated state: measured - predicted(x_{k|k}) to first order in x_{k|k} - x_{k|k-1}.

## Smoother

`smooth(steps)` is a Rauch-Tung-Striebel smoother over the steps of a forward filter pass. With x_{k|k} the filtered
state on plane k, x_{k+1|k} the state predicted on plane k+1 from plane k, F_{k+1} the transport Jacobian from plane k
to plane k+1 and N the last plane:

```
G_k     = C_{k|k} F_{k+1}^T (C_{k+1|k})^-1
x_{k|N} = x_{k|k} + G_k (x_{k+1|N} - x_{k+1|k})
C_{k|N} = C_{k|k} + G_k (C_{k+1|N} - C_{k+1|k}) G_k^T
```

starting from x_{N|N}, C_{N|N} (the last filtered state) and going backwards. Each `filter_step` stores what the
smoother needs at its plane: `predicted` (x_{k|k-1}, C_{k|k-1}), `filtered` (x_{k|k}, C_{k|k}) and `F` (the Jacobian
from the previous plane; unused for the first step). Since the predicted covariance is stored as it was used by the
filter, process noise added to it in the prediction would be included automatically; there is none yet, as the
library has no material model. The function returns nothing if a predicted covariance cannot be inverted.

## Engine

Header `engine.hpp`. `sand::tracker::kalman::engine` is the complex data object of the library. The seeder, the
combinatorial track finder, the fit and smoother and the vertex fit all need the same state: magnetic field,
reconstruction calibration and propagation settings. Keeping that state in one global, unique complex data object means
it is configured once per job and every module sees the same instance; and since the library is compiled once into
`sand_tracker_kalman`, every module runs the same code, instead of each one compiling its own copy of header-only
functions (which duplicates code and can give slightly different numerical results).

The engine is configured in the `"globals"` section of the job. Every key is required (see
section "Configuration: no defaults in the code"):

```json
"globals" : {
  "sand::tracker::kalman::engine" : {
    "field" : [0.6, 0.0, 0.0],
    "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0,
                      "time_resolution" : 3.5, "t0_offset" : 0.0 },
    "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 }
  }
}
```

| Parameter Name                | Type        | Unit  | Required/Default | Description                                                     |
|-------------------------------|-------------|-------|------------------|-----------------------------------------------------------------|
| `field`                       | `double[3]` | T     | Required         | Uniform magnetic field (Bx, By, Bz).                            |
| `calibration.drift_velocity`  | `double`    | mm/ns | Required         | Drift velocity assumed by the reconstruction.                   |
| `calibration.wire_velocity`   | `double`    | mm/ns | Required         | Signal propagation speed along the wire.                        |
| `calibration.time_resolution` | `double`    | ns    | Required         | Total resolution sigma_t on the measured time (see "wire_time"). |
| `calibration.t0_offset`       | `double`    | ns    | Required         | Fixed electronics offset.                                       |
| `propagator.max_step`         | `double`    | mm    | Required         | Maximum Runge-Kutta step.                                       |
| `propagator.max_slope`        | `double`    |       | Required         | Propagation fails if \|tx\| or \|ty\| exceed it.                  |

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
auto const upd  = update(prop->out, pred, tdc, cal.time_resolution * cal.time_resolution);
steps.push_back({prop->out, upd.out, prop->F});
current = upd.out;
```

`update()` is called here without the weight argument, which defaults to 1: a normal measurement. The rule of no
default values (section "Configuration: no defaults in the code") applies to configuration parameters, not to function
arguments; the weight is set by the caller, and only the deterministic annealing filter passes a different value.

## Tests

The standalone tests are in `tests/standalone/kalman/`:

- `kalman_propagator`: helix in a uniform field against the analytic solution (radius R = p_{perp} / (kappa |q| B),
  with p_{perp} the momentum component perpendicular to the field, time of flight with the muon mass from the table),
  round trip forward and back, Jacobian against finite differences in both directions, particle moving towards
  smaller z, fourth-order convergence in a non-uniform field (the error ratio when the step is halved is about 16;
  it is 4 if stage 3 reuses the field of stage 2), straight track with a time of flight independent of the mass,
  rejection of unset propagation settings;
- `kalman_filter`: closest approach distance; analytic derivatives of the time model against Richardson-extrapolated
  central differences on 2000 random states and wires (straight and stereo, both sides, relative tolerance 1e-6 plus
  the rounding bound of the differences, which only matters for derivatives close to zero); checks of the
  `wire_geometry` constructor; `mass_from_pdg` against `TDatabasePDG`; `update()` rejecting a weight that is zero,
  negative or NaN, and the default weight equal to 1; pulls of the fitted parameters on toy tracks (section
  "Validation so far");
- `kalman_engine`: engine built from a JSON configuration, configured values, an error naming each missing key,
  rejection of invalid configurations (including the old key name `tdc_resolution`), `engine.propagate` and
  `engine.wire_time` identical to the free functions with the same field, settings and calibration.

The tests link `sand_tracker_kalman` and `ROOT::EG`.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun and needs an edep-sim file; build it against `sand_tracker_kalman` (include path `src/data/tracker` and the
Geant4 include directory for CLHEP), ROOT (with `libEG`) and the edep-sim I/O library.

## Validation so far

### Propagator against the edep-sim truth

`gun_propagation_check` on the particle-gun samples simulated with the geometry `nd_hall_with_lar_tms_sand_drift1`
(uniform 0.6 T along x inside the magnet). Each sample has 1000 events of one particle shot along +z from the centre of
SAND, with the kinetic energy given in the tables. For each pair of consecutive true trajectory points of the primary
inside the tracker (a cylinder of radius 1.5 m around the SAND axis, |x| < 1.5 m), the true state at the first point is
propagated to the z of the second and compared with the true state there (propagated minus true). The momentum of the
start state is the true momentum at the first point.

Reference sample, μ⁻ of 1 GeV kinetic energy: 13231 pairs of points. The propagation distance between the two points of
a pair is 106.8 mm on average in z (rms 60.2 mm), 107.8 mm along the track (rms 60.8 mm). The statistical error on a
mean is rms / sqrt(13231), about 1.9 μm for dx and dy (an underestimate, since the pairs of the same track are
correlated).

| Propagator field | dx mean [μm] | dx rms [μm] | dy mean [μm] | dy rms [μm] | dty mean | dty rms | dt mean, rms [ps] |
|------------------|--------------|-------------|--------------|-------------|----------|---------|-------------------|
| 0.6 T (on)       | +1.4 ± 1.9   | 213         | -6.3 ± 1.9   | 222         | +0.00006 | 0.0027  | below 0.1, 0.1    |
| 0 (off)          | +1.4 ± 1.9   | 213         | +1280 ± 8    | 966         | +0.0184  | 0.0104  | -0.5, 0.5         |

"Field off" means the propagator run without field (B = 0) on the same truth, which was simulated with the field: it
shows the size of the bending over the steps (y is the bending coordinate), which the propagation with the field
removes. x is not bent, so dx is the same in the two rows. The rms of dx and dy with the field on is multiple
scattering and energy-loss fluctuations, which are not modelled (no material model yet).

All 20 samples, field on (dx, dy in μm; the error on the mean is rms / sqrt(pairs)):

| Sample        | Pairs | dx mean | dx rms | dy mean | dy rms |
|---------------|-------|---------|--------|---------|--------|
| μ⁻ 500 MeV    | 12859 | -2.1    | 347    | -11.9   | 337    |
| μ⁻ 1 GeV      | 13231 | +1.4    | 213    | -6.3    | 222    |
| μ⁻ 1.5 GeV    | 13320 | +0.4    | 186    | -6.9    | 192    |
| μ⁻ 2 GeV      | 10635 | +1.4    | 198    | -2.9    | 188    |
| μ⁺ 500 MeV    | 12684 | -1.9    | 328    | +14.3   | 355    |
| μ⁺ 1 GeV      | 13250 | +2.7    | 216    | +9.7    | 274    |
| μ⁺ 1.5 GeV    | 12663 | -0.6    | 190    | +3.9    | 196    |
| μ⁺ 2 GeV      | 11581 | +0.2    | 175    | -1.2    | 178    |
| π⁻ 500 MeV    | 10285 | -7.7    | 742    | -21.8   | 1133   |
| π⁻ 1 GeV      | 8807  | +1.9    | 235    | -3.1    | 232    |
| π⁻ 1.5 GeV    | 6302  | -6.0    | 269    | -11.0   | 268    |
| π⁻ 2 GeV      | 5212  | -3.1    | 268    | -5.9    | 354    |
| π⁺ 500 MeV    | 10558 | +18.4   | 1383   | -5.5    | 1605   |
| π⁺ 1 GeV      | 8637  | -1.5    | 415    | -3.7    | 1071   |
| π⁺ 1.5 GeV    | 6305  | -1.5    | 303    | +1.1    | 414    |
| π⁺ 2 GeV      | 5245  | -2.9    | 253    | +2.2    | 258    |
| p 100 MeV     | 6459  | -25.3   | 867    | +43.1   | 1016   |
| p 200 MeV     | 12060 | +4.7    | 618    | +30.9   | 784    |
| p 500 MeV     | 9463  | -3.0    | 330    | +15.5   | 361    |
| p 750 MeV     | 8333  | +0.5    | 254    | +10.3   | 260    |

The pion samples have a larger rms, from a few pairs with large deviations (probably hadronic interactions of the
primary; not investigated). The mean dy is not zero within its statistical error for several samples: its sign follows
the charge and it decreases with the momentum roughly like the bending itself, about 0.5% of the bending over a step
for the muons (-6.3 μm against the +1280 μm of the field-off row at 1 GeV). This is the behaviour of a field slightly
lower on average than the uniform 0.6 T of the propagator along the selected points; energy loss, which the
propagator ignores, would give the opposite sign. It is not yet understood and will be revisited with the field map
read from the GDML.

The time of flight agrees within 0.1 ps (rms) for all samples. Reading the field at every Runge-Kutta stage does not
change any of these numbers, because the field of the check is uniform.

### Toy fits

`pulls_on_toy_tracks` in `kalman_filter`: tracks generated with the same propagator (no material) in a uniform 0.6 T
field, 120 planes every 10 mm in three views (0, +5 and -5 degrees), wire pitch 10 mm, TDC time from the `wire_time`
model smeared with a Gaussian of width `time_resolution` = 3.5 ns, momentum 600 MeV plus a half-Gaussian of 400 MeV,
alternating charge, the true side for every hit (so there is no left-right ambiguity). The pull of a parameter is
(fitted - true) / sqrt(C_{ii}). With `n_toys` toy tracks the statistical error is `1/sqrt(n_toys)` on the mean of a pull
and about `1/sqrt(2 n_toys)` on its width.

ctest (`n_toys` = 300; errors 0.058 on the mean, 0.041 on the width):

| Parameter | Filtered, last plane: mean | width | Smoothed, first plane: mean | width |
|-----------|----------------------------|-------|-----------------------------|-------|
| x         | -0.026                     | 1.019 | +0.045                      | 1.094 |
| y         | -0.119                     | 0.985 | -0.006                      | 1.004 |
| tx        | -0.040                     | 1.044 | -0.039                      | 1.045 |
| ty        | -0.078                     | 1.040 | +0.028                      | 1.045 |
| q/p       | -0.060                     | 1.052 | -0.060                      | 1.052 |
| t         | -0.028                     | 1.024 | -0.027                      | 1.023 |

All widths are within 3 standard deviations of 1; the largest deviation is the width of the smoothed x, 1.094, at 2.3
standard deviations. The largest mean is y on the last plane, -0.119, at 2.1 standard deviations. The q/p pulls are the
same on the two planes: without material q/p is constant along the track, and the smoothed value on the first plane is
the filtered value on the last.

The same study with `n_toys` = 3000 (run outside ctest, which keeps 300 for its run time; errors 0.018 on the mean,
0.013 on the width):

| Parameter | Filtered, last plane: mean | width | Smoothed, first plane: mean | width |
|-----------|----------------------------|-------|-----------------------------|-------|
| x         | -0.037                     | 1.018 | +0.035                      | 0.995 |
| y         | -0.038                     | 1.005 | -0.017                      | 1.001 |
| tx        | -0.041                     | 1.004 | -0.041                      | 1.004 |
| ty        | -0.040                     | 0.995 | +0.034                      | 1.005 |
| q/p       | -0.038                     | 0.995 | -0.038                      | 0.995 |
| t         | +0.000                     | 1.001 | +0.001                      | 1.001 |

All widths are within 1.5 standard deviations of 1, so the 1.094 of the 300-track sample is a fluctuation. The means are
within 2.3 standard deviations of 0; the pulls of the parameters of one track are strongly correlated, so the twelve
entries are not independent tests.
