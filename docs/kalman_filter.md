# Kalman filter library for the tracker

This page describes the Kalman filter library of the SAND tracker: what it computes, with which formulas, and where in
the code. It assumes that the reader knows what a track fit is, but not this code.

## 0. Introduction and conventions

This section says what the library does and fixes the conventions used in the rest of the page: units, reference
frame, field convention and notation.

### 0.1 What the library does

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field, the measurement models for a single
wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the measurement model
applies to drift chambers and straw tubes alike, and is the same model used by the fast digitizations
`drift_fast_digi` and `stt_fast_digi`.

What the library does not do:

- no pattern recognition: it does not decide which digits belong to a track, nor the left-right hypothesis of a
  wire; the caller gives it the measurements of one track, one at a time;
- no digit reading and no geometry access: it knows nothing about digits or about the geometry service. The
  reconstruction modules read the digits and the wire descriptions, build the inputs described below and call the
  library, so a change in the digit format or in the geometry only affects the modules;
- no configuration in the functions: they take field, calibration and settings as arguments. The shared
  configuration (field, reconstruction calibration, propagation settings) is held by the engine, a complex data
  object configured once per job (section 3).

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/particle.hpp>`,
`<kalman/propagator.hpp>`, `<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/field.hpp>` and
`<kalman/engine.hpp>`. The implementation is in `.cpp` files and depends on ROOT `SMatrix`, on the CLHEP units of the
Geant4 installation (header only), on ROOT's `TDatabasePDG` (for `hypothesis_from_pdg`) and on ufw (for the engine
configuration).

The document reads top to bottom: section 1 summarises the Kalman filter and names the vectors and matrices it
uses; section 2 describes each of them, with its formula, the code that computes it, its approximations and its
tests; sections 3 to 5 cover the configuration, the planned extensions and the validation; appendix A relates the
state to the helix parametrisation of other SAND track fits. The Doxygen documentation
of each function points to its section here.

### 0.2 Units and reference frame

Before any formula, the units and the frame in which the library expresses every quantity it takes or returns.

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

All positions, directions and field values are in the global frame of the geometry. The z axis plays a special role:
the tracker measures the trajectory on planes of constant z, and z is the independent variable of the track
description (section 2.1).

### 0.3 Field and curvature constant kappa

The field is the one quantity whose unit differs from the CLHEP internal one; this section says how the library
handles it, and why the curvature constant kappa is derived rather than written.

The magnetic field is a number in tesla everywhere in the library: in the `field` interface, in the engine
configuration and in the values read from the GDML. It is not converted to the CLHEP internal unit (where
`CLHEP::tesla` = 0.001). The curvature constant `kappa` is therefore derived, not written:

```
kappa = CLHEP::c_light * CLHEP::tesla = 0.299792458   [MeV / (mm T)]

p [MeV] = kappa * |q| * B [T] * R [mm]        (q in units of e)
```

Mass and absolute charge are an argument of the library (the particle hypothesis); code that needs them for a species
gets them from the particle table (section 2.2).

### 0.4 Notation

With units and constants fixed, this section lists every symbol used in the formulas below, with its dimension and
the name it has in the code.

The formulas are written in plain text so that they read the same in GitLab and in Doxygen: `a . b` is the scalar
product, `a x b` the vector product, `|a|` the norm, `M^T` the transpose, `M^-1` the inverse.

Each symbol has one meaning in the whole document and in the Doxygen documentation. Subscripts are written with braces
when they are more than one character (`x_{k|k}`); a subscript makes a different symbol (`v` and `v_d`). The state
vector is written with a plane label or an argument (`x_{k|j}`, `x(z)`), the coordinate x without. In the dimension
column. In the "dimension" column, n means a column vector with n components (the state x is 6), n x m a matrix (C and
F are 6 x 6; H is 1 x 6, a row), and an empty cell a scalar (r, V). Rows that group vectors and scalars say "scalar"
explicitly. The "name in the code" column gives either a member of a library struct (`prediction::H`) or a local
variable followed by its function in parentheses (`S` (`update()`) is the variable `S` inside `update()`).

| Symbol | Meaning | Unit | Dimension | Name in the code |
|--------|---------|------|-----------|------------------|
| x, y, z | global coordinates; z is also the independent variable and labels the planes | mm | | `state::par[X]`, `state::par[Y]`, `state::z` |
| tx, ty | slopes dx/dz, dy/dz | | | `state::par[TX]`, `state::par[TY]` |
| q, \|q\| | charge, in units of e; its absolute value, part of the particle hypothesis | e | | sign: `state::charge()`; `particle_hypothesis::abs_charge` |
| p, p_{perp} | momentum, p = \|q\| / \|q/p\|; its component perpendicular to the field | MeV | | `momentum(state, particle_hypothesis)`; — |
| q/p | charge over momentum | 1/MeV | | `state::par[QOP]` |
| t | time at which the track crosses a plane | ns | | `state::par[T]` |
| m | mass of the particle hypothesis | MeV | | `particle_hypothesis::mass`; argument `hyp` of `propagate()`, from `hypothesis_from_pdg()` |
| beta, E | velocity over c; energy | MeV (E) | | 1/beta: `g` (`detail::derivatives()`) |
| c, kappa | `CLHEP::c_light`; curvature constant `CLHEP::c_light * CLHEP::tesla` | mm/ns, MeV/(mm T) | | `CLHEP::c_light`; `kappa` (`propagator.cpp`) |
| B = (Bx, By, Bz) | magnetic field | T | 3 | `field_vector`, returned by `field::at()` |
| sigma | sign of p_z: the `direction` of the state | | | `state::direction` |
| x(z), x_{k\|j} | state vector (x, y, tx, ty, q/p, t) at z; on plane k, using the measurements up to plane j | | 6 | `state::par` (`vector_t`) |
| C, C_{k\|j} | covariance of the state vector | | 6 x 6 | `state::cov` (`sym_matrix_t`) |
| k, N | plane index; index of the last plane | | | index in the vector of `filter_step` given to `smooth()` |
| f_k | propagator: the state on plane k as a function of the state on plane k - 1 | | 6 -> 6 | `propagate()` |
| F, F_k | propagation matrix (transport Jacobian) df_k/dx; from plane k - 1 to plane k | | 6 x 6 | `propagation_result::F`, `filter_step::F` (`matrix_t`) |
| Q, Q_k | process noise matrix; added on plane k (zero in this version) | | 6 x 6 | — |
| v, \|v\| | track direction (tx, ty, 1), not normalised; its norm sqrt(1 + tx^2 + ty^2) | | 3 | \|v\|: `nv` (`detail::derivatives()`) |
| tau | unit tangent to the track, sigma v / \|v\| | | 3 | `state::direction_cosines()` |
| s | path length along the track | mm | | `propagation_result::path_length` |
| P, P0 | point on the track; track point on the wire plane | mm | 3 | P0: (`par[X]`, `par[Y]`, `z`) |
| lambda, mu | parameters along the local track line and along the wire | mm | | — |
| phi, A | right-hand side dx(z)/dz of the equations of motion; its Jacobian dphi/dx | | 6; 6 x 6 | outputs `f`, `A` of `detail::derivatives()` |
| phi_x, phi_y, phi_tx, phi_ty, phi_t | components of phi: dx/dz, dy/dz, dtx/dz, dty/dz, dt/dz | | | `f[X]`, ... (`detail::derivatives()`) |
| a_x, a_y | field terms of dtx/dz and dty/dz (section 2.3) | T | | `ax`, `ay` (`detail::derivatives()`) |
| step | Runge-Kutta step | mm | | `h` (`propagate()`) |
| phi_i, A_i, B_i, J_i | phi, A and field at Runge-Kutta stage i; stage i of the Jacobian | | 6; 6 x 6; 3; 6 x 6 | `f1`..`f4`, `A1`..`A4`, `b1`..`b4`, `J1`..`J4` (`propagate()`) |
| R | radius of the helix | mm | | — (used in the tests) |
| W0, W(mu) | readout end (head) of a wire; point on the wire, W0 + mu u | mm | 3 | `wire_geometry::head()`; — |
| u = (ux, uy, 0) | unit vector along the wire, from W0 to the far end | | 3 | `wire_geometry::along()` |
| n = (-uy, ux) | in-plane unit normal to the wire | | 2 | `nx`, `ny` (`wire_time()`) |
| Delta | P0 - W0 | mm | 3 | `dx`, `dy` (`detail::wire_distance()`) |
| t_u, t_n | slopes along and across the wire | | | `tu`, `tn` (`wire_time()`) |
| a, b | u . Delta, n . Delta | mm | | `du` (`detail::wire_distance()`), `b` (`wire_time()`) |
| rho | sqrt(1 + t_n^2) | | | `rho` (`wire_time()`) |
| d | signed distance of closest approach between track and wire | mm | | output `d` of `detail::wire_distance()` |
| L | distance from W0 of the point of the wire closest to the track | mm | | output `L` of `detail::wire_distance()` |
| side | left-right hypothesis, +1 or -1 | | | argument `side` of `wire_time()` |
| v_d, v_w | drift velocity; signal speed along the wire | mm/ns | | `reco_calibration::drift_velocity`, `reco_calibration::wire_velocity` |
| t0_offset | fixed electronics offset | ns | | `reco_calibration::t0_offset` |
| t_{TDC} | TDC time | ns | | `prediction::value` of `wire_time()`; argument `measured` of `update()` |
| sigma_t, sigma_{TDC}, sigma_x | total time resolution (`time_resolution`); TDC resolution; intrinsic spatial resolution | ns, ns, mm | | `reco_calibration::time_resolution`; `sigma_tdc` of the fast digitizations; — |
| h_k | measurement prediction on plane k: the predicted value as a function of the state | | 6 -> 1 | `wire_time()`, `wire_position()` |
| measured, predicted | measured value; value predicted by a measurement model, h_k(x) | | | argument `measured` of `update()`; `prediction::value` |
| H, H_k | projection matrix: derivative row d(predicted)/dx of a measurement model; on plane k | | 1 x 6 | `prediction::H` (`row_t`, stored as a 6-vector) |
| V, w | measurement variance (time_resolution^2 for `wire_time`); weight of the measurement | | 1 x 1; scalar | arguments `variance` and `weight` of `update()` |
| r, S, K | residual; its variance; Kalman gain | | scalar; scalar; 6 | `update_result::residual`; `S` (`update()`); `K` (`update()`) |
| e_{k\|j}, eps | error of the state estimate x_{k\|j} - x (x the true state); error of the measurement (section 2.8) | | 6; scalar | — |
| chi2 | chi2 increment of a measurement | | | `update_result::chi2` |
| I | identity matrix | | 6 x 6 | `ROOT::Math::SMatrixIdentity` |
| G_k | smoother gain on plane k | | 6 x 6 | `Gk` (`smooth()`) |
| R~, lambda_dip, phi_h | helix parameters: signed radius, dip angle, azimuth around the axis of the helix (appendix A) | mm, , | | — |
| zc, yc, x0, phi_{h0} | axis of the helix in the bending plane; x and phi_h at a reference point of the helix (appendix A) | mm, mm, mm, | | — (used in the tests) |

## 1. The Kalman filter in brief

With the conventions of section 0 fixed, this section outlines the algorithm: the four steps of the filter, the
vectors and matrices each one uses, and the function that implements it.

A track fit estimates the state x of the track, with its covariance C, from the measurements of the wires it crosses.
The Kalman filter does it recursively: it follows the track one measurement site (plane) at a time, and on each plane it
combines the estimate carried from the previous planes with the measurement of that plane. After the last plane N, the
filtered state is the best estimate there, using all the measurements; a backward pass, the smoother, then gives the
best estimate on every plane. For a linear model the filter is equivalent to a least-squares fit of the whole track, but
it only ever handles 6 x 6 matrices and a scalar measurement at a time, and it can follow a trajectory that is not a
simple function (a helix in a non-uniform field, energy loss). A general introduction, including the smoother, is in
reference [1], and its application to track fitting in reference [2] (section 6).

Each plane k goes through the first three steps; the fourth runs once, backwards, at the end.

**1. Propagation.** The filtered state of plane k - 1 is carried to plane k:

```
x_{k|k-1} = f_k(x_{k-1|k-1})
C_{k|k-1} = F_k C_{k-1|k-1} F_k^T + Q_k
```

f_k is the propagator (section 2.2), F_k = df_k/dx its 6 x 6 Jacobian, the propagation matrix (section 2.3), and Q_k
the 6 x 6 process noise matrix of the material crossed (section 2.4; zero in this version).

**2. Measurement prediction.** The measurement model gives the value expected on plane k for the predicted state, and
its derivative row:

```
predicted = h_k(x_{k|k-1})
H_k       = dh_k/dx at x_{k|k-1}            (1 x 6)
```

h_k is the wire model (section 2.5), H_k the projection matrix (section 2.6).

**3. Filtering (update).** The measurement, with variance V (section 2.7) and weight w, updates the state (section
2.8):

```
r       = measured - predicted                                       residual
S       = H_k C_{k|k-1} H_k^T + V / w                                residual variance
K       = C_{k|k-1} H_k^T / S                                        gain (6 x 1)
x_{k|k} = x_{k|k-1} + K r                                            updated state
C_{k|k} = (I - K H_k) C_{k|k-1} (I - K H_k)^T + K (V / w) K^T        updated covariance (Joseph form)
chi2    = r^2 / (H_k C_{k|k-1} H_k^T + V)                            chi2 increment
```

**4. Smoothing.** After the last plane, a Rauch-Tung-Striebel backward pass (section 2.9) updates every plane with
the measurements downstream of it, from k = N - 1 down to the first plane:

```
G_k     = C_{k|k} F_{k+1}^T (C_{k+1|k})^-1
x_{k|N} = x_{k|k} + G_k (x_{k+1|N} - x_{k+1|k})
C_{k|N} = C_{k|k} + G_k (C_{k+1|N} - C_{k+1|k}) G_k^T
```

| Step | Inputs | Outputs | Implemented by |
|------|--------|---------|----------------|
| 1. Propagation | x_{k-1\|k-1}, C_{k-1\|k-1}, z of plane k, particle hypothesis, field | x_{k\|k-1}, C_{k\|k-1}, F_k, path length | `propagate()` (`propagator.hpp`), `engine::propagate()` |
| 2. Measurement prediction | x_{k\|k-1}, wire, side, calibration | predicted, H_k | `wire_time()`, `wire_position()` (`measurement.hpp`), `engine::wire_time()` |
| 3. Filtering | x_{k\|k-1}, C_{k\|k-1}, predicted, H_k, measured, V, w | x_{k\|k}, C_{k\|k}, r, chi2, residual after the update | `update()` (`filter.hpp`) |
| 4. Smoothing | for every plane: x_{k\|k-1}, C_{k\|k-1}, x_{k\|k}, C_{k\|k}, F_k | x_{k\|N}, C_{k\|N} | `smooth()` (`filter.hpp`), with the steps stored as `filter_step` |

The loop over the planes, the choice of the measurements and of the left-right hypothesis are the job of the
reconstruction modules; section 3.3 shows one step of a forward fit.

## 2. Building blocks

Section 1 named the vectors and matrices of the filter; this section describes each of them in turn, in the order in
which the filter uses them.

Each section gives the definition, the formula and its derivation, the implementation (file and function), the
approximations and the tests that check it. The tests are described in section 5.

### 2.1 State x and covariance C

The state is what the filter estimates and what every other building block takes as input; this section defines it
and the consequences of using z as the independent variable.

**Definition.** The state is defined on a plane of constant `z` (header `state.hpp`, struct `state`):

| Index | Parameter | Meaning |
|-------|-----------|---------|
| `X`, `Y` | x, y | position on the plane [mm] |
| `TX`, `TY` | tx, ty | slopes dx/dz, dy/dz |
| `QOP` | q/p | charge over momentum [1/MeV], q in units of e |
| `T` | t | time at which the particle crosses the plane [ns] |

```
x = (x, y, tx, ty, q/p, t)        at the plane z
```

The time is part of the state, so the fit determines the track time together with the trajectory. This is needed
inside the 10 μs spill, where the time of each track is not known in advance. For the same reason the measurement
of the final fit is the raw TDC time and not a drift distance computed before the fit, which would need the track time
from outside (section 2.5, "Two measurement models"). The track t0, in the sense used by the reconstruction, is the
value of t on the first (or vertex) plane.

The covariance `C` is the symmetric 6 x 6 matrix of the same parameters, in the same order and units
(`sym_matrix_t`).

**Direction and z as the independent variable.** The independent variable is `z`: the trajectory is described by its
crossings of planes at fixed z, which is where the tracker measures it. Consequences:

- the state also carries `direction` = sigma = sign(p_z), +1 for a particle moving towards larger z and -1 towards
  smaller z. The slopes keep their meaning as dx/dz and dy/dz in both cases; the direction of motion is
  tau = sigma v / |v|, with v = (tx, ty, 1);
- a track parallel to the planes (p_z = 0) cannot be represented: its slopes are infinite. The propagation stops
  when |tx| or |ty| exceed `propagator_config::max_slope`, which happens for tracks that curl back in the field;
- the direction is a property of the particle, not of the propagation: propagating a state to a plane behind it
  runs the equations backwards and keeps `direction`.

**Charge and momentum.** The state carries q/p, with q the signed charge in units of e, and not p: the curvature
depends on q/p only (section 2.2), so the state describes the trajectory of a particle of any charge. The momentum
needs the absolute charge |q|, which is part of the particle hypothesis (section 2.2):

```
p = |q| / |q/p|
```

It is computed by the free function `momentum(state, particle_hypothesis)`, which uses only the `abs_charge` of the
hypothesis. It is a free function of the state and the hypothesis, rather than a member of `state`, because the state
alone does not know |q|: an alpha (|q| = 2) and a unit charge with the same q/p have momenta that differ by a factor 2.
`state::charge()` returns the sign of q/p, that is the sign of the charge.

**Straight tracks.** A straight track has q/p = 0. `momentum()` returns `HUGE_VAL` for it and `state::charge()`
returns 0; its time of flight is described in section 2.2.

**Implementation.** `state.hpp`: struct `state` (`z`, `par`, `cov`, `direction`, and the helpers `charge()`,
`direction_cosines()`), the free function `momentum()`, the index enum `par` and the matrix types `vector_t`,
`matrix_t`, `sym_matrix_t`, `row_t`. The identity matrix is `ROOT::Math::SMatrixIdentity`
(`matrix_t I(ROOT::Math::SMatrixIdentity{})`, or `F = ROOT::Math::SMatrixIdentity()`).

**Approximations.** None: the state is exact for any trajectory that crosses the planes with p_z != 0.

**Tests.** `backward_moving_particle` (direction -1), `straight_track_time_independent_of_mass` (q/p = 0) and
`alpha_time_of_flight_and_momentum` (momentum for |q| = 2) in `kalman_propagator`.

### 2.2 Propagator f

The propagator carries the state of section 2.1 from one plane to the next (step 1 of section 1); this section
derives the equations of motion and their integration.

**Definition.** The propagator f_k gives the state on plane k from the state on plane k - 1 by integrating the
equations of motion of a charged particle in the magnetic field. `propagate(state, z_target, hyp, field, config)`
moves a state to another plane with a fourth-order Runge-Kutta in z and transports its covariance (section 2.3).

**Equations of motion.** For a particle of charge q (in units of e) and momentum p in a static magnetic field, with s
the path length, P the position and `tau = dP/ds` the unit tangent, the Lorentz force gives

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
dt/dz   = (ds/dz) / (beta c) = sigma * |v| * sqrt(1 + (m/|q|)^2 (q/p)^2) / c
```

where m and |q| are the mass and the absolute charge of the particle hypothesis, and

```
1/beta = E/p = sqrt(1 + m^2/p^2),    p = |q| / |q/p|    =>    1/beta = sqrt(1 + m^2 (q/p)^2 / q^2)
```

These six derivatives form the right-hand side phi(x, z) = dx(z)/dz.

**Where the charge enters.** The curvature terms dtx/dz and dty/dz depend on q/p only: they are valid for any charge,
and |q| does not appear in them. The particle hypothesis enters only the time of flight, and there only through the
ratio m/|q|: an alpha (|q| = 2) has the time of flight of a unit charge of mass m/2 with the same q/p. For |q| = 1 the
formula is the usual sqrt(1 + m^2 (q/p)^2).

**Straight tracks.** For q/p = 0 the time of flight uses 1/beta = sqrt(1 + (m/|q|)^2 * 0) = 1, that is beta = 1, and
the particle hypothesis has no effect: dt/dz = sigma |v| / c.

**Runge-Kutta integration.** With the state vector x(z) and the right-hand side phi(x, z) above, each step of length
step (|step| <= `max_step`, the interval being divided into equal steps) is the classical fourth-order Runge-Kutta
(reference [3]):

```
phi_1 = phi(x(z), z)
phi_2 = phi(x(z) + step/2 phi_1, z + step/2)
phi_3 = phi(x(z) + step/2 phi_2, z + step/2)
phi_4 = phi(x(z) + step phi_3,   z + step)
x(z + step) = x(z) + step/6 (phi_1 + 2 phi_2 + 2 phi_3 + phi_4)
```

The field B_i of each stage is read at the position (x, y) of the stage state and its z, so there are four field
queries per step. The two middle stages are at different positions (they differ by order step^2), so reading the
field at both is what makes the scheme a true fourth-order Runge-Kutta also in a non-uniform field: with the field of
stage 2 reused for stage 3 the error would decrease only as step^2 (the test `fourth_order_in_non_uniform_field`
checks the order). For a uniform field the two are identical. A negative step propagates towards smaller z.

**Time of flight and path length.** The time t is integrated with the other components, so `propagate()` returns
the time on the target plane for the particle hypothesis. The path length is accumulated with the trapezoidal rule on
ds/dz over each step:

```
path_length += |step| * (|v(z)| + |v(z + step)|) / 2
```

It is exact for a straight track. It is returned in `propagation_result::path_length`, but nothing in the library uses
it yet: it is meant for a material model (energy loss and multiple scattering, sections 2.4 and 4.2), which will use
it when it is added.

**Field.** Header `field.hpp`. `field` is an interface returning the field (Bx, By, Bz) in T at a position in mm, in
the global frame (`field::at()`). `uniform_field` is the only implementation for now; the field read from the
geometry will be a second implementation of the same interface (section 4.3).

**Particle hypothesis.** The struct `particle_hypothesis` (`particle.hpp`) holds the mass m [MeV] and the absolute
charge |q| [e] of the particle assumed by the fit; it is an argument of `propagate()` and `engine::propagate()`, and
`momentum()` takes it too. `propagate()` throws `std::invalid_argument` if |q| is not positive or m is negative (or
either is NaN). Code that needs the hypothesis of a species calls `hypothesis_from_pdg(pdg)`, which reads ROOT's
`TDatabasePDG`: the mass in GeV, converted with `CLHEP::GeV`, and the charge in units of |e|/3, converted to units of e
(|q| = |Charge()| / 3). It caches the values and throws `std::invalid_argument` for an unknown code and for a neutral
particle. The default table of ROOT has no nuclei: a nucleus such as the alpha (1000020040) is found only if the job
has added it to `TDatabasePDG` (as the test does); otherwise the caller builds the `particle_hypothesis` directly.

**Implementation.** `propagator.hpp`, `propagator.cpp`: `propagate()` does the Runge-Kutta loop and returns a
`propagation_result` (`out`, `F`, `path_length`), or nothing if the track becomes too steep (|tx| or |ty| above
`max_slope`); `detail::derivatives()` computes phi (output `f`) and A, with m/|q| in the local `mq`. `propagate()`
throws `std::invalid_argument` if `max_step` or `max_slope` is not positive, as in a value-initialised
`propagator_config`. `particle.hpp`, `particle.cpp`: `particle_hypothesis`, `hypothesis_from_pdg()`.

**Approximations.** Energy loss and multiple scattering are not included: the library has no material model yet
(sections 2.4 and 4.2). The charge of the particle is any: the curvature uses q/p and the time of flight m/|q|.

**Tests.** In `kalman_propagator`: `helix_in_uniform_field`, `alpha_time_of_flight_and_momentum` (|q| = 2 against
the exact kinematics), `round_trip`, `backward_moving_particle`, `fourth_order_in_non_uniform_field`,
`straight_track_time_independent_of_mass`, `hypothesis_is_checked`, `settings_are_required`; in `kalman_filter`:
`hypotheses_from_pdg`. The comparison with the edep-sim truth is in section 5.2.

### 2.3 Propagation matrix F

The propagator of section 2.2 moves the state; its Jacobian F moves the covariance, and is computed in the same
integration.

**Definition.** F_k = df_k/dx is the 6 x 6 Jacobian of the state on plane k with respect to the state on plane
k - 1, the transport Jacobian. It carries the covariance from one plane to the next and links the planes in the
smoother.

**Formula and derivation.** The Jacobian `F = d x(z1) / d x(z0)` satisfies

```
dF/dz = A(x(z), z) F,    F(z0) = I,    A = dphi/dx
```

and is integrated with the same Runge-Kutta stages, using the A_i of each stage (computed with the field B_i of that
stage):

```
J_1 = A_1 F
J_2 = A_2 (F + step/2 J_1)
J_3 = A_3 (F + step/2 J_2)
J_4 = A_4 (F + step J_3)
F(z + step) = F + step/6 (J_1 + 2 J_2 + 2 J_3 + J_4)
```

**Elements of A.** A = dphi/dx is computed analytically. With |v| = sqrt(1 + tx^2 + ty^2), the field terms

```
a_x = tx ty Bx - (1 + tx^2) By + ty Bz
a_y = (1 + ty^2) Bx - tx ty By - tx Bz
```

and the components of phi (section 2.2)

```
phi_tx = sigma kappa (q/p) |v| a_x
phi_ty = sigma kappa (q/p) |v| a_y
phi_t  = sigma |v| sqrt(1 + (m/|q|)^2 (q/p)^2) / c
```

using d|v|/dtx = tx / |v|, d|v|/dty = ty / |v|, and the derivatives of a_x and a_y with respect to the slopes, the
non-zero elements are

```
dphi_x/dtx  = 1
dphi_y/dty  = 1

dphi_tx/dtx    = sigma kappa (q/p) [ (tx/|v|) a_x + |v| (ty Bx - 2 tx By) ]
dphi_tx/dty    = sigma kappa (q/p) [ (ty/|v|) a_x + |v| (tx Bx + Bz) ]
dphi_tx/d(q/p) = sigma kappa |v| a_x

dphi_ty/dtx    = sigma kappa (q/p) [ (tx/|v|) a_y + |v| (-ty By - Bz) ]
dphi_ty/dty    = sigma kappa (q/p) [ (ty/|v|) a_y + |v| (2 ty Bx - tx By) ]
dphi_ty/d(q/p) = sigma kappa |v| a_y

dphi_t/dtx     = sigma (tx/|v|) sqrt(1 + (m/|q|)^2 (q/p)^2) / c
dphi_t/dty     = sigma (ty/|v|) sqrt(1 + (m/|q|)^2 (q/p)^2) / c
dphi_t/d(q/p)  = sigma |v| (m/|q|)^2 (q/p) / (c sqrt(1 + (m/|q|)^2 (q/p)^2))
```

All the other elements are zero: q/p is constant (no energy loss), so its row is zero, and phi does not depend on x,
y and t once the field gradients are neglected, so their columns are zero. In the code, `ax`, `ay` are a_x, a_y, `nv`
is |v|, `kq` is sigma kappa (q/p) and `g` is the square root; the terms in parentheses are `dax_dtx`, `dax_dty`,
`day_dtx`, `day_dty`.

The covariance is transported as

```
C(z1) = F C(z0) F^T
```

that is C_{k|k-1} = F_k C_{k-1|k-1} F_k^T + Q_k with Q_k = 0 (section 2.4).

**Implementation.** `propagate()` integrates F together with the state and returns it in `propagation_result::F`;
the transported covariance is `propagation_result::out.cov`. `A` is computed analytically by `detail::derivatives()`.
The caller keeps F in `filter_step::F` for the smoother.

**Approximations.** The field is read at every stage, but its derivatives with respect to the position (the field
gradients) are neglected in A, which is exact for a uniform field.

**Tests.** `derivatives_vs_finite_differences` in `kalman_propagator` checks every element of A above against central
differences of phi (general field, both directions, unit charge and alpha), and that the zero elements are exactly
zero; `jacobian_vs_finite_differences` checks the integrated F (both directions); the pulls of the toy fits (section
5.3) check the transported covariance as a whole.

### 2.4 Process noise matrix Q

F (section 2.3) transports the covariance along a deterministic trajectory; Q is the second term of the propagated
covariance, for the random effects of the material.

**Definition.** Q_k is the 6 x 6 covariance added to the predicted state on plane k for the random effects of the
material crossed between plane k - 1 and plane k: multiple scattering, which spreads the slopes (and, through them,
the positions), and the fluctuations of the energy loss, which spread q/p.

**This version.** The library has no material model, so Q_k = 0: the propagation only transports the covariance,
C_{k|k-1} = F_k C_{k-1|k-1} F_k^T, and the propagator does not change q/p. The formulas for multiple scattering and
energy loss, from the Review of Particle Physics (reference [4]), will be described here when the material model is
added (section 4.2). They will use the path length returned by `propagate()` (section 2.2).

**Implementation.** None yet. The smoother uses the predicted covariance as stored by the filter (section 2.9), so a
Q added in the propagation step will be taken into account without changes there.

**Tests.** None yet. The rms of the comparison with the edep-sim truth (section 5.2) shows the size of the effects
that Q will describe.

### 2.5 Measurement prediction h

With the state propagated to a wire plane (sections 2.2 to 2.4), the measurement prediction (step 2 of section 1)
gives the value the wire should measure; this section describes the wire models.

**Definition.** The measurement prediction h_k gives the value that the wire of plane k should measure for a given
state on its plane. Two models are provided (header `measurement.hpp`):

- `wire_time`: the TDC time, used in the final fit;
- `wire_position`: the coordinate of the track across the wire, used in track finding.

Both need the state on the wire plane (`state.z == wire.z()`) and return a `prediction`: the predicted value and its
derivative row `H` = d(value)/d(state) (section 2.6).

**Two measurement models.** The two models measure different quantities of the same wire hit (the formulas are given
below in this section):

| | `wire_time` | `wire_position` |
|-|-------------|-----------------|
| Measured value | the TDC time of the digit | 0: the track passes on the wire |
| Prediction | t_{TDC} = t + L / v_w + side * d / v_d + t0_offset | n . Delta |
| Units of the residual | ns | mm |
| Variance V (section 2.7) | sigma_t^2, with sigma_t = `time_resolution` | flat over the cell |
| Needs the track time | yes: t enters the prediction | no |
| Used in | final fit | track finding |

The filter does not know what the measurement is: `update()` (section 2.8) needs only the measured value, the
prediction h with its row H, and V. The residual r and its variance S have the units of the measurement (ns and ns^2,
or mm and mm^2), and the gain K = C H^T / S converts them to the units of the state: K_i has the units of parameter i
divided by those of the measurement, so K r is a change of the state. The same `update()` therefore serves both
models. Each update is independent of the others, so the two models could even be mixed in one fit, one per wire;
the reconstruction does not do it.

*Why the raw TDC time is the measurement.* An alternative is to convert the TDC time into a distance from the wire (a
drift radius) before the fit, and to fit the radii. The conversion needs the track time, to subtract it from the TDC
time, and the position along the wire, to subtract the propagation time of the signal L / v_w. Neither is known in
advance in the spill (section 2.1). In the `wire_time` model they are the state parameter t and the quantity L
computed from the state, so the fit estimates them together with the trajectory instead of taking them from outside.

*Linear and non-linear model.* `wire_position` is exactly linear in the state, n . Delta = nx (x - W0x) + ny (y - W0y):
its H is exact for any state, and the update with it is a true linear Kalman filter, with no linearisation error.
`wire_time` is non-linear in the slopes (through rho and the product t_n t_u in d and L) and is linearised around the
predicted state: the update with it is an extended Kalman filter (section 2.8).

**Wire geometry.** A wire is described by `wire_geometry`, built from its two ends in the global frame: `head` is the
readout end W0 and `tail` the far end. The models assume that the wire lies in a plane of constant z, so the
constructor checks it: it throws `std::invalid_argument` if `|head.z - tail.z|` exceeds `s_wire_plane_tolerance` =
1 μm (`1 * CLHEP::micrometer`), or if the wire has zero length. The tolerance is a geometric sanity check (the
geometry describes wires in planes of constant z, up to rounding), not a physics or configuration value. Any number
of views and any stereo angle are handled in the same way.

The geometric quantities:

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

**Distance of closest approach d.** The signed distance of closest approach between the track line and the wire line
is the projection of Delta on their common normal v x u:

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

**Position along the wire L.** L is the distance from the readout end of the point of the wire closest to the track:
the value of mu that minimises `|P0 + lambda v - W0 - mu u|^2` over lambda and mu. Setting both derivatives to zero,

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

the form used for the derivatives (section 2.6). The denominator rho^2 = 1 + t_n^2 is at least 1.

**TDC time model (`wire_time`).** The TDC time is predicted as

```
t_{TDC} = t + L / v_w + side * d / v_d + t0_offset
```

with t the track time on the wire plane, v_w the signal speed along the wire, v_d the drift velocity and t0_offset a
fixed electronics offset (all from `reco_calibration`).

**Why side * d.** The drift time is |d| / v_d: the electrons drift from the closest approach point to the wire
whichever side the track passes. But |d| is not differentiable at d = 0, and the Kalman update needs the derivative.
The model therefore uses side * d, with the left-right hypothesis `side` (+1 or -1) fixed during the update:

- with the right hypothesis (side = sign of d) the term equals |d| / v_d: the drift time is never subtracted;
- with the wrong hypothesis it predicts a negative drift time, and the residual is about 2 |d| / v_d. It is large only
  far from the wire, where the deterministic annealing filter (section 4.1), which treats the two sides as two
  competing measurements, gives the wrong hypothesis a low weight. Close to the wire, where 2 |d| / v_d is comparable
  with the time resolution sigma_t, the two hypotheses cannot be told apart; there the choice also matters little,
  since both predict nearly the same time.

The model is smooth across the wire for each hypothesis, which is what the annealing needs.

The parameters of `reco_calibration` (drift velocity, wire velocity, time resolution, time offset) are what the
reconstruction assumes. They are kept separate from the parameters of the digitization, even when they have the
same values.

**Coordinate model (`wire_position`).** The signed coordinate of the track across the wire, on the wire plane:

```
value = n . Delta,    H = (nx, ny, 0, 0, 0, 0)
```

Used in track finding, with the wire position as measurement (value 0) and a flat error (section 2.7).

**Implementation.** `measurement.hpp`, `measurement.cpp`: `wire_geometry`, `reco_calibration`, `prediction`,
`wire_time()`, `wire_position()`; `detail::wire_distance()` computes d and L. `engine::wire_time()` calls
`wire_time()` with the calibration of the engine.

**Approximations.** The track is a straight line near the wire plane (the curvature over a drift cell is negligible);
the drift is linear in the distance (constant drift velocity); the time of flight from the plane to the closest
approach point is neglected.

**Tests.** In `kalman_filter`: `distance_to_wire`, `wire_geometry_checks`, `wire_position_is_linear` (H independent of
the state and h(x + dx) = h(x) + H dx for large changes of position and slopes); in `kalman_engine`:
`wire_time_matches_free_function`.

### 2.6 Projection matrix H

The update needs, besides the predicted value of section 2.5, its derivative with respect to the state; this section
gives it analytically.

**Definition.** H_k = dh_k/dx is the 1 x 6 derivative row of the measurement model with respect to the state on the
wire plane: it projects a change of the state onto a change of the predicted measurement. It is returned with the
predicted value in `prediction::H`.

**Formula and derivation.** H is computed analytically from the side-fixed model of section 2.5. With a = u . Delta,
b = n . Delta and rho = sqrt(1 + t_n^2):

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

For `wire_position` the row is H = (nx, ny, 0, 0, 0, 0) (section 2.5).

**Implementation.** `wire_time()` and `wire_position()` in `measurement.cpp` fill `prediction::H` (`row_t`, a
6-vector used as a row).

**Approximations.** Those of the model (section 2.5): H is exact for the side-fixed straight-line model; the side is
not a differentiable parameter and stays fixed.

**Tests.** In `kalman_filter`: `time_derivatives` (dt_{TDC}/dt = 1, dt_{TDC}/d(q/p) = 0, and the derivative across
the wire equal to side / (rho v_d) up to the small term from L) and `time_derivatives_vs_finite_differences` (the
whole row against Richardson-extrapolated central differences of the same model, section 5.1).

### 2.7 Measurement noise V

The last input of the update, after the predicted value (section 2.5) and its derivative (section 2.6), is the
variance of the measurement; this section says what it contains.

**Definition.** V is the variance of the measured value, a scalar (1 x 1) for the single-wire measurements of this
library. It is an argument of `update()`: the caller supplies it.

**TDC time.** The variance of the measured time in the update is sigma_t^2, where sigma_t is
`reco_calibration::time_resolution` (key `calibration.time_resolution`): the total resolution on the measured time,
the TDC resolution plus the intrinsic spatial resolution of the cell divided by the drift velocity,

```
V = sigma_t^2 = sigma_{TDC}^2 + (sigma_x / v_d)^2
```

The current fast digitizations (`drift_fast_digi`, `stt_fast_digi`) smear only the TDC time, with their `sigma_tdc`,
so with them sigma_t = sigma_{TDC}. A separate spatial term sigma_x will be added when the digitization models it.

**Wire coordinate.** For `wire_position` the measured value is the wire position (0) and the variance is flat over
the cell: pitch^2 / 12 or max_radius^2 / 3.

**Weight.** The weight w of the measurement scales the variance as V / w in the gain (section 2.8).

**Implementation.** `reco_calibration::time_resolution`, configured in the engine (section 3.2); the caller passes
`cal.time_resolution * cal.time_resolution` as the `variance` argument of `update()` (section 3.3).

**Approximations.** sigma_t is the same for every wire and does not depend on the distance from the wire.

**Tests.** The pulls of the toy fits (section 5.3), which smear the TDC time with sigma_t and fit with V = sigma_t^2.

### 2.8 Gain K and update

This section combines the predicted state (sections 2.2 to 2.4) with the measurement (sections 2.5 to 2.7): it is
step 3 of section 1, the update of the state on the measurement plane.

**Definition.** `update(state, prediction, measured, variance, weight)` adds one scalar measurement with variance V to
the predicted state (x_{k|k-1}, C_{k|k-1}) on the measurement plane k. The prediction gives the predicted value,
predicted = h_k(x_{k|k-1}), and the row H. The gain K is the 6 x 1 vector that sets how much the residual moves each
parameter.

**Formula.**

```
r = measured - predicted(x_{k|k-1})               residual
S = H C_{k|k-1} H^T + V / w                       variance of the residual
K = C_{k|k-1} H^T / S                             gain (a 6-vector)
x_{k|k} = x_{k|k-1} + K r
C_{k|k} = (I - K H) C_{k|k-1} (I - K H)^T + K (V / w) K^T
```

**Weight.** The weight w scales the measurement variance as V / w: a measurement with a small weight pulls the state
less. It is the hook for the deterministic annealing filter (section 4.1), where each wire side enters with its own
weight in (0, 1]; w = 1 is the ordinary Kalman update, and is the default value of the argument.

**Weight contract.** The weight must be positive. Weights above 1 are accepted: they scale the measurement variance
down (V / w < V), as for a more precise measurement; in the deterministic annealing filter the weights are in (0, 1].
With w = 0 the variance V / w is infinite and the covariance becomes NaN (0 times infinity in the Joseph form); a
negative weight is a negative variance. `update()` therefore throws `std::invalid_argument` for w <= 0 and for NaN,
instead of returning a corrupted state. A caller that gives a
measurement no weight must skip the update: the deterministic annealing filter, which drops the hypotheses below its
minimum weight, never calls `update()` with w = 0 (it is not part of the library yet; this is the contract it will
follow).

**Joseph form: derivation.** Let x be the true state on plane k, e_{k|k-1} = x_{k|k-1} - x the error of the prediction
and eps = measured - h_k(x) the error of the measurement, with zero mean and

```
<e_{k|k-1} e_{k|k-1}^T> = C_{k|k-1},    <eps^2> = V,    <e_{k|k-1} eps> = 0     (prediction and measurement independent)
```

To first order, r = measured - h_k(x_{k|k-1}) = eps - H e_{k|k-1}, so the error of the updated state is

```
e_{k|k} = x_{k|k} - x = e_{k|k-1} + K (eps - H e_{k|k-1}) = (I - K H) e_{k|k-1} + K eps
```

and its covariance, the cross terms vanishing because e_{k|k-1} and eps are independent, is

```
C_{k|k} = <e_{k|k} e_{k|k}^T> = (I - K H) C_{k|k-1} (I - K H)^T + K V K^T          (Joseph form)
```

Nothing in this derivation uses the value of K: the Joseph form is the covariance of the updated state for any gain.

**Equivalent forms.** With S = H C_{k|k-1} H^T + V and the optimal gain K = C_{k|k-1} H^T S^-1 (S is a scalar here, so
S^-1 = 1/S), the Joseph form reduces to two other forms in common use. Expanding it:

```
C_{k|k} = C_{k|k-1} - K H C_{k|k-1} - C_{k|k-1} H^T K^T + K (H C_{k|k-1} H^T + V) K^T
        = C_{k|k-1} - K H C_{k|k-1} - C_{k|k-1} H^T K^T + K S K^T
```

and for the optimal gain K S K^T = C_{k|k-1} H^T S^-1 S K^T = C_{k|k-1} H^T K^T, so the last two terms cancel:

```
C_{k|k} = C_{k|k-1} - K H C_{k|k-1} = (I - K H) C_{k|k-1}                          (short form)
```

The short form is also the information form

```
C_{k|k} = [ C_{k|k-1}^-1 + H^T V^-1 H ]^-1                                         (information form)
```

To show it, multiply the bracket by the short form and use C_{k|k-1}^-1 K = H^T S^-1 and
H K = H C_{k|k-1} H^T S^-1 = (S - V) S^-1 = I - V S^-1:

```
[ C_{k|k-1}^-1 + H^T V^-1 H ] (C_{k|k-1} - K H C_{k|k-1})
  = I - H^T S^-1 H C_{k|k-1} + H^T V^-1 H C_{k|k-1} - H^T V^-1 (I - V S^-1) H C_{k|k-1}
  = I - H^T S^-1 H C_{k|k-1} + H^T V^-1 H C_{k|k-1} - H^T V^-1 H C_{k|k-1} + H^T S^-1 H C_{k|k-1}
  = I
```

This is the Woodbury identity, [P^-1 + H^T V^-1 H]^-1 = P - P H^T (V + H P H^T)^-1 H P, with P = C_{k|k-1}.

**Conditions of the equivalence.** The three forms are equal when

- the gain is the optimal one, K = C_{k|k-1} H^T S^-1 (the Joseph form holds for any K; the short and the information
  forms only for this one);
- for the information form, C_{k|k-1} and V are invertible;
- the arithmetic is exact.

With a weight, V becomes V / w everywhere: in S, in the second term of the Joseph form and in the information form
(V^-1 becomes w V^-1).

**Why the Joseph form.** The library updates the covariance in Joseph form. It is a sum of two positive semidefinite
terms, so in exact arithmetic it is positive semidefinite for any gain; and since it is correct for any K, a gain
perturbed by rounding gives an error of second order in the perturbation, where the short form has a first-order error.
The short form subtracts two nearly equal matrices when the measurement is precise, and under rounding it can lose
symmetry and positive definiteness. The information form needs two 6 x 6 inversions (C_{k|k-1}^-1, then the bracket)
and is fragile when C_{k|k-1} is very large, as in a fit started from an inflated prior: C_{k|k-1}^-1 is then tiny,
H^T V^-1 H has rank 1, and the bracket is nearly singular in the directions not yet measured. The Joseph form is not
immune to rounding, but it is much more robust than the other two. The result is also symmetrised explicitly, as
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

**Implementation.** `filter.hpp`, `filter.cpp`: `update()` returns an `update_result` (`out`, `residual`, `chi2`,
`residual_after`); the variance of the residual is the local `S`, the gain `K`, and I - K H the local `IKH`. The code
is written in matrix form, as the formulas: the outer products K H and K K^T are `ROOT::Math::TensorProd(K, H)` and
`ROOT::Math::TensorProd(K, K)`, so that `IKH = I - TensorProd(K, H)` and `C += v * TensorProd(K, K)`, with v = V / w.
The symmetrisation is `detail::symmetrize()`, which returns (C + C^T) / 2 as a `sym_matrix_t`: it is a helper
because SMatrix does not allow assigning a general matrix to a symmetric one, so the copy into the symmetric
covariance must be written element by element; taking the mean of C and C^T, rather than one triangle, also removes
the rounding asymmetries of the product. The formulas are those of references [1] and [2].

**Approximations.** The measurement model is linearised around x_{k|k-1} (extended Kalman filter): the update is
exact for a linear model, such as `wire_position`, and for the non-linear `wire_time` the residual after the update is
first order (section 2.5, "Two measurement models").

**Tests.** In `kalman_filter`: `update_rejects_non_positive_weight` (weight zero, negative or NaN rejected, default
weight equal to 1, a small weight pulls less, a weight above 1 pulls more, chi2 independent of the weight) and
`covariance_forms_agree` (Joseph, short and information forms equal for the optimal gain, with and without a weight),
`symmetrize_takes_symmetric_part` ((C + C^T) / 2 exactly, a symmetric matrix unchanged bit for bit);
the pulls of the toy fits (section 5.3).

### 2.9 Smoother

The updates of section 2.8, plane after plane, give the best estimate on the last plane only; the smoother (step 4 of
section 1) carries the information of all the measurements back to every plane.

**Definition.** `smooth(steps)` is a Rauch-Tung-Striebel smoother (reference [1]) over the steps of a forward filter
pass: it gives on every plane the estimate that uses all the measurements of the track, upstream and downstream.

**Formula.** With x_{k|k} the filtered state on plane k, x_{k+1|k} the state predicted on plane k+1 from plane k,
F_{k+1} the transport Jacobian from plane k to plane k+1 and N the last plane:

```
G_k     = C_{k|k} F_{k+1}^T (C_{k+1|k})^-1
x_{k|N} = x_{k|k} + G_k (x_{k+1|N} - x_{k+1|k})
C_{k|N} = C_{k|k} + G_k (C_{k+1|N} - C_{k+1|k}) G_k^T
```

starting from x_{N|N}, C_{N|N} (the last filtered state) and going backwards.

**What `filter_step` stores.** Each `filter_step` stores what the smoother needs at its plane: `predicted`
(x_{k|k-1}, C_{k|k-1}), `filtered` (x_{k|k}, C_{k|k}) and `F` (F_k, the Jacobian from the previous plane; unused for
the first step). Since the predicted covariance is stored as it was used by the filter, process noise added to it in
the prediction would be included automatically; there is none yet, as the library has no material model (section
2.4).

**Implementation.** `smooth()` in `filter.cpp`; the gain is the local `Gk`, the difference C_{k+1|N} - C_{k+1|k}
the local `dC`, and the smoothed covariance is symmetrised with `detail::symmetrize()`, as in the update (section
2.8). The function returns nothing if a predicted covariance cannot be inverted.

**Approximations.** Those of the filter: the smoother uses the Jacobians of the forward pass, linearised around the
filtered states.

**Tests.** The smoothed pulls on the first plane in `pulls_on_toy_tracks` (section 5.3).

## 3. Engine and configuration

The building blocks of section 2 take the field, the calibration and the propagation settings as arguments; this
section describes the engine that holds them for a whole job, and how it is configured.

### 3.1 The complex data object

This section says why the shared state lives in one complex data object, and what that object offers.

Header `engine.hpp`. `sand::tracker::kalman::engine` is the complex data object of the library. The seeder, the
combinatorial track finder, the fit and smoother and the vertex fit all need the same state: magnetic field,
reconstruction calibration and propagation settings. Keeping that state in one global, unique complex data object means
it is configured once per job and every module sees the same instance; and since the library is compiled once into
`sand_tracker_kalman`, every module runs the same code, instead of each one compiling its own copy of header-only
functions (which duplicates code and can give slightly different numerical results).

The engine exposes

- `field()`, `calibration()`, `propagator_settings()`: the shared state;
- `propagate(state, z_target, hyp)`: `propagate` with the field and settings of the engine, for the particle
  hypothesis `hyp`;
- `wire_time(state, wire, side)`: `wire_time` with the calibration of the engine.

The free functions stay available with explicit arguments (the engine calls them), for tests and for code that needs
a different field or calibration.

The field is uniform for now. It is owned by the engine through a pointer to the `field` interface, so the field map
read from the GDML will replace it without changing the modules (section 4.3).

### 3.2 Configuration

The engine of section 3.1 is built from the job configuration; this section lists its keys.

The engine is configured in the `"globals"` section of the job. Every key is required (section 3.4):

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
| `calibration.time_resolution` | `double`    | ns    | Required         | Total resolution sigma_t on the measured time (section 2.7).    |
| `calibration.t0_offset`       | `double`    | ns    | Required         | Fixed electronics offset.                                       |
| `propagator.max_step`         | `double`    | mm    | Required         | Maximum Runge-Kutta step.                                       |
| `propagator.max_slope`        | `double`    |       | Required         | Propagation fails if \|tx\| or \|ty\| exceed it.                  |

Velocities, resolution, step and slope must be positive. A missing key is an error that names it
(`missing required key 'calibration.drift_velocity'`), and unknown keys are rejected, so a misspelt key is an error
and not a silent default.

### 3.3 How a process gets it

With the engine configured (section 3.2), a reconstruction module gets it and calls the library as below.

A process gets the engine with `instance<sand::tracker::kalman::engine>()` and links `sand_tracker_kalman` in its
CMakeLists. One step of a forward fit, as a reconstruction module would write it (steps 1 to 3 of section 1):

```cpp
using namespace sand::tracker::kalman;

auto const& kf  = instance<engine>();
auto const& cal = kf.calibration();
auto const hyp  = hypothesis_from_pdg(13); // muon hypothesis: mass and |q| = 1

wire_geometry const w(head, tail);   // ends from the wire description of the geometry service
double tdc;                          // from the digit
int side;                            // left-right hypothesis

auto prop = kf.propagate(current, w.z(), hyp);
if (!prop) {
  // track lost
}
auto const pred = kf.wire_time(prop->out, w, side);
auto const upd  = update(prop->out, pred, tdc, cal.time_resolution * cal.time_resolution);
steps.push_back({prop->out, upd.out, prop->F});
current = upd.out;
```

After the last plane, `smooth(steps)` gives the smoothed states (step 4).

### 3.4 Configuration: no defaults in the code

The configuration of section 3.2 has no optional keys; this section states the rule behind that and its limits.

Every tunable parameter comes from the configuration; the code has no default values. The structs that carry the
parameters (`reco_calibration`, `propagator_config`) have no default member values, `propagate()` has no default
settings argument (it throws if `max_step` or `max_slope` is not positive, as in a value-initialised
`propagator_config`), and every key of the engine configuration is required: a missing key is an error that names
it. Code that builds the structs directly (tests, standalone tools) sets every member.

The rule applies to configuration parameters, not to function arguments: the weight argument of `update()` defaults to
1, a normal measurement (section 2.8). In the example above `update()` is called without it; the weight is set by the
caller, and only the deterministic annealing filter passes a different value.

Physical constants are not configuration and are not written as numbers either: they come from CLHEP (sections 0.2
and 0.3) and masses and charges from the particle table (`hypothesis_from_pdg`).

The only fixed number in the library is `s_wire_plane_tolerance` (section 2.5), a geometric sanity check of the wire
description, not a physics or configuration value.

### 3.5 Public interface and namespace detail

Besides the engine and the functions used above, the headers declare a few helpers; this section says which ones a
module may call.

Everything in `sand::tracker::kalman` is the public interface, used by the reconstruction modules.

The nested namespace `sand::tracker::kalman::detail` holds helpers that are not part of that interface:

- `detail::derivatives()`: right-hand side of the equations of motion and its Jacobian, used by `propagate()`;
- `detail::wire_distance()`: distance of closest approach and position along the wire, used by `wire_time()`;
- `detail::symmetrize()`: symmetric part (C + C^T) / 2 of a general matrix, used by `update()` and `smooth()` to store
  the new covariance (section 2.8).

Modules should not call them. They are declared in the headers, rather than hidden in the `.cpp` files, only because
the standalone tests check them directly against analytic results, finite differences and exact expectations.

## 4. Extensions

The subsections below are placeholders for the parts of the track fit that are planned but are not part of this
version of the library. Later merge requests fill them in place.

### 4.1 Deterministic annealing filter

The left-right hypothesis of each wire is fixed in this version (section 2.5); this subsection is about the filter
that will choose it.

The deterministic annealing filter will resolve the left-right ambiguity by treating the two sides of each wire as
competing measurements, each entering `update()` with its own weight (section 2.8; reference [5]); it is not part
of this version.

### 4.2 Material effects

This version has no material (Q = 0, section 2.4); this subsection is about the model that will add it.

The material model will add multiple scattering and energy-loss fluctuations to the process noise Q (section 2.4) and
the mean energy loss to the propagator (section 2.2), using the path length; it is not part of this version.

### 4.3 Field from the geometry

The field of this version is uniform (sections 2.2 and 3.1); this subsection is about the field read from the
geometry.

The field map read from the BField auxiliary tags of the GDML will be a second implementation of the `field`
interface, owned by the engine in place of the uniform field; it is not part of this version.

## 5. Tests and validation

After the formulas (section 2) and their configuration (section 3), this section says how they are checked: the
standalone tests run by ctest, the comparison of the propagator with the simulation, and the toy fits.

### 5.1 Standalone tests

The tests below are cited in the "Tests" part of each building block of section 2; here they are grouped by test
executable.

The standalone tests are in `tests/standalone/kalman/` and are run by ctest. They link `sand_tracker_kalman` and
`ROOT::EG`.

`kalman_propagator` (sections 2.2 and 2.3):

- `helix_in_uniform_field`: helix in a uniform field against the analytic solution (radius
  R = p_{perp} / (kappa |q| B), with p_{perp} the momentum component perpendicular to the field, time of flight with
  the muon mass from the table);
- `alpha_time_of_flight_and_momentum`: an alpha (|q| = 2, added to the particle table by the test) against the exact
  kinematics: radius, time of flight with 1/beta = sqrt(1 + m^2/p^2), momentum |q| / |q/p|, and the same time of
  flight as a unit charge of mass m/2;
- `derivatives_vs_finite_differences`: every element of A = dphi/dx (section 2.3) against central differences of phi,
  on 200 random states (general field, both directions, muon, alpha and proton), and the zero elements exactly zero;
- `helix_parametrisation`: the relations of appendix A between the state and the helix parameters, against the
  analytic helix and the propagator;
- `round_trip`: propagation forward and back to the start plane;
- `jacobian_vs_finite_differences`: the Jacobian F against finite differences, in both directions;
- `backward_moving_particle`: a particle moving towards smaller z;
- `fourth_order_in_non_uniform_field`: fourth-order convergence in a non-uniform field (the error ratio when the step
  is halved is about 16; it is 4 if stage 3 reuses the field of stage 2);
- `straight_track_time_independent_of_mass`: straight track with a time of flight independent of the mass;
- `hypothesis_is_checked`: rejection of a particle hypothesis with |q| <= 0, negative mass or NaN;
- `settings_are_required`: rejection of unset propagation settings.

`kalman_filter` (sections 2.5 to 2.9):

- `distance_to_wire`: closest approach distance and position along the wire;
- `time_derivatives`: dt_{TDC}/dt = 1, dt_{TDC}/d(q/p) = 0 and the derivative across the wire;
- `time_derivatives_vs_finite_differences`: analytic derivatives of the time model against Richardson-extrapolated
  central differences on 2000 random states and wires (straight and stereo, both sides, relative tolerance 1e-6 plus
  the rounding bound of the differences, which only matters for derivatives close to zero);
- `wire_geometry_checks`: checks of the `wire_geometry` constructor;
- `wire_position_is_linear`: the row H of `wire_position` independent of the state, and h(x + dx) = h(x) + H dx for
  large changes of the position and of the slopes (section 2.5);
- `hypotheses_from_pdg`: `hypothesis_from_pdg` against `TDatabasePDG` (mass, |q| = 1 for the charged leptons, pions,
  kaons and protons, exception for unknown codes and neutral particles);
- `update_rejects_non_positive_weight`: `update()` rejecting a weight that is zero, negative or NaN, the default
  weight equal to 1, and a weight above 1 accepted;
- `covariance_forms_agree`: the Joseph, short and information forms of the covariance update (section 2.8) equal for
  the optimal gain;
- `symmetrize_takes_symmetric_part`: `detail::symmetrize()` equal to (C + C^T) / 2 element by element, and an exactly
  symmetric matrix returned unchanged, bit for bit;
- `pulls_on_toy_tracks`: pulls of the fitted parameters on toy tracks (section 5.3).

`kalman_engine` (section 3):

- `configured_values`: engine built from a JSON configuration, configured values;
- `missing_keys`: an error naming each missing key;
- `invalid_configurations`: rejection of invalid configurations (including the old key name `tdc_resolution`);
- `propagate_matches_free_function`, `wire_time_matches_free_function`: `engine.propagate` and `engine.wire_time`
  identical to the free functions with the same field, settings and calibration.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun (section 5.2) and needs an edep-sim file; build it against `sand_tracker_kalman` (include path `src/data/tracker`
and the Geant4 include directory for CLHEP), ROOT (with `libEG`) and the edep-sim I/O library.

### 5.2 Propagator against the edep-sim truth

The standalone tests compare the propagator with analytic solutions; this section compares it with the trajectories
of a full simulation, in which the particle also crosses material.

`gun_propagation_check` on the particle-gun samples simulated with the geometry `nd_hall_with_lar_tms_sand_drift1`
(uniform 0.6 T along x inside the magnet). Each sample has 1000 events of one particle shot along +z from the centre of
SAND, with the kinetic energy given in the tables. For each pair of consecutive true trajectory points of the primary
inside the tracker (a cylinder of radius 1.5 m around the SAND axis, |x| < 1.5 m), the true state at the first point is
propagated to the z of the second and compared with the true state there (propagated minus true). The momentum of the
start state is the true momentum at the first point; the particle hypothesis comes from `hypothesis_from_pdg()` and
q/p = sign(q) |q| / p. All the samples have |q| = 1, and the numbers below did not change when the charge was added to
the hypothesis.

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
scattering and energy-loss fluctuations, which are not modelled (no material model yet, section 2.4).

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
primary; not investigated).

**Open: the dy bias.** The mean dy is not zero within its statistical error for several samples: its sign follows the
charge and it decreases with the momentum roughly like the bending itself, about 0.5% of the bending over a step for
the muons (-6.3 μm against the +1280 μm of the field-off row at 1 GeV). This is the behaviour of a field slightly
lower on average than the uniform 0.6 T of the propagator along the selected points; energy loss, which the
propagator ignores, would give the opposite sign. It is not yet understood and will be revisited with the field map
read from the GDML (section 4.3).

The time of flight agrees within 0.1 ps (rms) for all samples. Reading the field at every Runge-Kutta stage does not
change any of these numbers, because the field of the check is uniform.

### 5.3 Toy fits

Section 5.2 checks the propagator alone; the toy fits check the whole filter and smoother, including the covariances,
through the pulls of the fitted parameters.

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

## Appendix A. Relation to the helix parametrisation

Other SAND track fits describe a track in a uniform field along x by the helix parameters (x, y, 1/R~,
tan lambda_dip, phi_h) (phi_h is called phi in those fits; the subscript keeps it apart from the right-hand side phi of
section 2.2). This appendix derives the relations between them and the state of section 2.1.

**Helix.** Take the field B = (Bx, 0, 0) with Bx > 0, as in SAND. The bending plane is (z, y). From the equation of
motion of section 2.2, dtau/ds = kappa (q/p) (tau x B) with tau x B = (0, tau_z Bx, -tau_y Bx): the transverse
direction (tau_z, tau_y) rotates in the (z, y) plane, from z towards y when q > 0 (counterclockwise) and the other way
when q < 0, with radius R = p_{perp} / (kappa |q| Bx), p_{perp} = p cos lambda_dip the momentum component
perpendicular to the field. The component along the field is constant: p_x = p sin lambda_dip, which defines the dip
angle lambda_dip from the bending plane. With phi_h the azimuth of the track point around the axis of the helix
(zc, yc), and phi_{h0} its value at x = x0, the helix is

```
z = zc + R cos phi_h
y = yc + R sin phi_h
x = x0 + sign(q) R tan(lambda_dip) (phi_h - phi_{h0})
```

The particle moves towards increasing phi_h for q > 0 and decreasing phi_h for q < 0: its transverse arc length grows
as R dphi_h sign(q), and x grows by tan(lambda_dip) per unit of transverse arc length. The direction of motion is

```
(p_x, p_y, p_z) = p ( sin lambda_dip,  sign(q) cos lambda_dip cos phi_h,  -sign(q) cos lambda_dip sin phi_h )
```

and the signed curvature is 1/R~ = sign(q) / R. (For Bx < 0, replace sign(q) by sign(q Bx) everywhere and Bx by |Bx|
in R.)

**Slopes.** Differentiating the helix with respect to phi_h, dz/dphi_h = -R sin phi_h, dy/dphi_h = R cos phi_h and
dx/dphi_h = sign(q) R tan(lambda_dip), so

```
ty = (dy/dphi_h) / (dz/dphi_h) = -cot phi_h
tx = (dx/dphi_h) / (dz/dphi_h) = -sign(q) tan(lambda_dip) / sin phi_h
```

and the direction of the state is sigma = sign(p_z) = -sign(q) sign(sin phi_h). The state cannot represent the points
with sin phi_h = 0, where the track is parallel to the planes (section 2.1).

**Charge over momentum.** From p cos lambda_dip = kappa |q| Bx R, with q in units of e:

```
q/p = sign(q) |q| cos(lambda_dip) / (kappa |q| Bx R) = cos(lambda_dip) (1/R~) / (kappa Bx)
```

|q| cancels: the helix fixes q/p, not p, as the curvature of section 2.2 depends on q/p only. The momentum then needs
the particle hypothesis, p = |q| / |q/p| (section 2.1).

**Inverse relations.** From a state (with sign(q) = sign(q/p)):

```
tan(lambda_dip) = sigma tx / sqrt(1 + ty^2)
phi_h           = atan2(-sign(q) sigma, sign(q) sigma ty)
1/R~            = (q/p) kappa Bx / cos(lambda_dip)
```

**Path length in a layer.** Since ds/dz = sigma |v| (section 2.2), a layer of thickness Delta z (small enough for v to
be constant across it) is crossed over the path length (Delta l in the helix fits)

```
Delta s = |Delta z| |v|,    |v| = sqrt(1 + tx^2 + ty^2) = 1 / (cos(lambda_dip) |sin phi_h|)
```

the last form from 1 + tan^2(lambda_dip) / sin^2(phi_h) + cot^2(phi_h) = (1 + tan^2(lambda_dip)) / sin^2(phi_h).
Between two points of the helix the path length is R |phi_{h1} - phi_{h0}| / cos(lambda_dip).

**Covariance update.** The helix fits update the covariance in the information form
C_{k|k} = [C_{k|k-1}^-1 + H^T V^-1 H]^-1. For the optimal gain it is equivalent to the Joseph form used by this
library (section 2.8), which is the more robust of the two under rounding.

**Check.** `helix_parametrisation` in `kalman_propagator` builds the state from the helix parameters with the relations
above (both charge signs, positive, negative and zero dip, four values of phi_h, a unit charge and an alpha),
propagates it with the Runge-Kutta propagator to another point of the helix, and compares it with the helix there:
position within 10 nm, slopes within 1e-8, path length within 1e-4 relative; it also checks |v|, the momentum and the
inverse relations.

## 6. References

The few external sources cited in the text, where the general methods used by the library are described in more
detail.

1. Wikipedia, "Kalman filter", https://en.wikipedia.org/wiki/Kalman_filter, including the Rauch-Tung-Striebel
   smoother (cited in sections 1, 2.8 and 2.9).
2. R. Fruhwirth, "Application of Kalman filtering to track and vertex fitting", Nucl. Instrum. Meth. A 262 (1987)
   444 (cited in sections 1 and 2.8).
3. Wikipedia, "Runge-Kutta methods", https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods (cited in section
   2.2).
4. Particle Data Group, S. Navas et al., "Review of Particle Physics", Phys. Rev. D 110 (2024) 030001, chapter
   "Passage of particles through matter", https://pdg.lbl.gov (cited in section 2.4, for the material effects).
5. R. Fruhwirth and A. Strandlie, "Track fitting with ambiguities and noise: a study of elastic tracking and
   nonlinear filters", Comput. Phys. Commun. 120 (1999) 197 (cited in section 4.1).
