# Kalman filter library for the tracker

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field and the material, the measurement
models for a single wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the
measurement model applies to drift chambers and straw tubes alike, and is the same model used by the fast
digitizations `drift_fast_digi` and `stt_fast_digi`.

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/propagator.hpp>`,
`<kalman/material.hpp>`, `<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/field.hpp>` and
`<kalman/engine.hpp>`. The
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
`F C F^T`. The target plane can be on either side of the starting one. In this overload the mass hypothesis enters
only the time of flight.

`propagate(state, z_target, mass, field, material, config)` also crosses the layers returned by the material budget
(see "Material" below). For each layer, in order, it propagates in the field to the layer centre; if the layer has a
transverse extent and the propagated (x, y) is outside it, it goes on without material, otherwise it changes q/p by
the energy loss and adds the scattering noise `Q`; then it propagates to `z_target`. The extent test is a step
function of the position and is not differentiated: the Jacobian ignores that a small change of the state could move
the track across an edge. The covariance is accumulated step by
step as `C <- F_k C F_k^T + Q_k`, so the returned covariance includes the noise, while the returned `F` is the
product of all the Jacobians (field and material steps) without noise, which is what the smoother expects. With no
layers in the interval the result is identical to the field-only overload.

Both overloads return nothing if the track becomes steeper than `propagator_config::max_slope`, which happens for
tracks that curl back in the field; the material overload also returns nothing if the particle stops in a layer.

## Material (`material.hpp`)

Material is a sequence of thin slabs perpendicular to z. A `layer` has its centre `z` and `thickness` along z [mm],
the radiation length `radiation_length` [mm], `density` [g/cm3], `z_over_a` [mol/g], `mean_excitation` I [MeV] and
`sternheimer`, the density effect parameters (`density_effect`: a, k, x0, x1, C-bar, delta0, as in the PDG tables of
atomic and nuclear properties). Left at its default, `sternheimer` disables the density effect correction. The
optional `extent` (`transverse_extent`: x_min, x_max, y_min, y_max in mm) limits the layer in x and y, for instance to
the station it belongs to; a layer without it covers the whole plane. The propagation applies a layer with an extent
only if the track, propagated in the field to the plane of the layer, crosses it inside the extent.

`material_budget` is an interface, like `field`: `crossed_layers(state, z_target)` returns the layers between the
plane of the state and `z_target`, ordered in the direction of propagation. The interval is half-open according to
the direction of motion: with `lo` and `hi` the smaller and larger of `s.z` and `z_target`, a layer is included if its
centre lies in `(lo, hi]` for `direction = +1` and in `[lo, hi)` for `direction = -1` (`layer_in_interval`).

In other words, a layer on a plane belongs to the segment that reaches that plane along the motion, and a state on a
plane is always physically downstream of the material on it. Propagating along the motion (forward filter), a layer
on the starting plane is excluded and one on the target plane included; propagating against the motion (backward
fit), the other way round. A propagation and its reverse cross the same layers, so the two passes agree on the
state at each plane, a round trip is the identity even with layers on the end planes, and no layer is counted twice.

The state is passed, not only its z, because an implementation stepping through the geometry needs position and
direction.

Implementations:

- `fixed_material_budget`: a fixed list of layers, returned as they are (with or without extent). Used in the tests.
- `station_material_budget` (`station_material.hpp`): the material of the tracker stations from
  `geoinfo::tracker_info`, see the next section. The engine owns one with `"material" : "tracker_info"`.
- planned: one stepping through the ROOT geometry along the track.

### Material of the tracker stations (`station_material.hpp`)

`station_material_budget` is built from a `geoinfo::tracker_info` (see `docs/geoinfo_tracker_materials.md`). At
construction every `material_layer` of every station is converted to a `layer` by `to_layer`: centre
`(z_min + z_max) / 2`, thickness `z_max - z_min`, the same X0, density, <Z/A> and I. The layers of all stations are
kept in one list sorted in z, each with the transverse extent of its station: the rectangle spanned in x and y by
the four corners `top_north`, `top_south`, `bottom_south`, `bottom_north`. The tracker description is not referenced
after construction.

`crossed_layers(s, z_target)` finds the layers in the interval by binary
search, applies the interval rule above (`layer_in_interval`) and returns them all, ordered along the propagation,
without looking at x and y. Whether the track crosses a station is decided by `propagate`, at the position propagated
in the field to the plane of each layer, so the curvature of the track is taken into account: a track that enters a
station only because it bends into it gets its material, one that bends out of it does not. Since the test uses the
position on the plane of the layer, a forward propagation and its reverse agree on it (to the precision of the
propagation). Inside the extent of a station the layers do not depend on x and y: they are those sampled by geoinfo
along one line per station.

The Sternheimer parameters in geoinfo refer to the density of the table (`reference_density`), not to the density of
the material in the geometry (graphite 2.23 g/cm3 against 2.265 in the table, C3H6 0.946 against 0.900). `to_layer`
rescales them to the geometry density with `rescale_density_effect`, `r = rho_geometry / rho_table`:

```
C  -> C  - ln r
x0 -> x0 - 0.5 log10 r
x1 -> x1 - 0.5 log10 r
a, k, delta0 unchanged
```

(Sternheimer, Berger and Seltzer, At. Data Nucl. Data Tables 30 (1984) 261). The plasma energy scales as sqrt(rho),
so `C = 2 ln(I / hbar omega_p) + 1` decreases by `ln r`, and the whole curve delta(X) moves to lower X by
`0.5 log10 r`: `delta_rho(X) = delta_table(X + 0.5 log10 r)`. Check: the porous graphite entry of Geant4
(`G4DensityEffectData` index 277, 1.70 g/cm3: a = 0.20762, k = 2.9532, x0 = 0.048, x1 = 2.5387, C = 3.155) rescaled
to 2.21 g/cm3 gives a = 0.2076, k = 2.9532, x0 = -0.0090, x1 = 2.4817, C = 2.8926. Materials without Sternheimer
parameters in geoinfo (the chamber gases) get no density effect correction.

The construction stops the job if a layer has a non-positive thickness, radiation length or density.

Speed: `propagate` integrates the field up to every layer centre. In the STT each C3H6 station has a transition
radiator of about 100 foils with air gaps, about 230 layers per station and 14834 in the tracker, so propagating a
muon through the whole STT with its material takes about 1.1 s (toy simulation and fit, against 0.05 s for the 645
layers of the drift chambers). Grouping the radiator foils into one equivalent layer per station is a follow-up.

The material step is done by `material_step(par, layer, mass, loss_sign)`, built from free functions that can be
used and tested on their own. With `N^2 = 1 + tx^2 + ty^2`:

- path length in the layer: `L = thickness * N`;
- Highland: `theta0 = 13.6 MeV / (beta p) * |q| * sqrt(L/X0) * (1 + 0.038 ln(L q^2 / (X0 beta^2)))`;
- noise on the slopes: `Q(tx,tx) = theta0^2 (1 + tx^2) N^2`, `Q(ty,ty) = theta0^2 (1 + ty^2) N^2`,
  `Q(tx,ty) = theta0^2 tx ty N^2`, all other elements zero;
- mean energy loss, Bethe-Bloch with the density effect correction:

  ```
  dE/dx = K q^2 (Z/A) rho / beta^2 * [ 0.5 ln(2 me beta^2 gamma^2 Tmax / I^2) - beta^2 - delta/2 ]
  Tmax  = 2 me beta^2 gamma^2 / (1 + 2 gamma me/M + (me/M)^2)

  X = log10(beta gamma)
  delta = 2 ln(10) X - C                        X >= x1
        = 2 ln(10) X - C + a (x1 - X)^k         x0 <= X < x1
        = delta0 10^(2 (X - x0))                X < x0  (zero for insulators)
  ```

  with `K = 0.307075` MeV cm2/mol and `me = 0.51099895` MeV (rho in g/cm3, so dE/dx comes out in MeV/cm and is
  converted to MeV/mm). The delta parametrisation is the one of Sternheimer, Berger and Seltzer, Atomic Data and
  Nuclear Data Tables 30 (1984) 261, with the parameters tabulated per material by the PDG (atomic and nuclear
  properties, muon energy loss tables). With the PDG parameters for graphite this reproduces the ionisation column of
  the PDG muon table within 0.05% around minimum ionisation. Without them the formula overestimates dE/dx in graphite
  by about 4% at 300 MeV/c, 10% at 1 GeV/c and 15% at 3 GeV/c for a muon, so the budget implementations should fill
  them.

The particle loses the energy when the propagation goes along its motion (`sign(z_target - s.z) == direction`) and
gains it when it goes against the motion, as in the backward fit and in the smoother (`energy_loss_sign`). The new
momentum gives the new q/p, with the charge sign unchanged. The time is not changed by the material step: the field
propagation already accounts for the path, with the momentum after each layer.

The energy after a layer solves `E1 = E0 - s L dE/dx(Em)` with `Em = (E0 + E1)/2` and `s = +1` for a loss, `-1` for a
gain (implicit midpoint, solved by fixed-point iteration). This scheme is symmetric: losing and then gaining the energy
in the same layer gives back the initial energy, so forward and backward propagation through the same layers are
inverse to each other. The scattering angle is evaluated at the same midpoint momentum.

The material Jacobian is the identity except for the q/p row. Its elements d(q/p)/d(q/p), d(q/p)/dtx and
d(q/p)/dty (the last two through the path length) are analytic given dE/dx and its derivative with respect to the
energy, which is taken by central differences. The noise is not differentiated.

What is neglected:

- shell corrections and higher-order terms of Bethe-Bloch (the PDG table includes them and agrees with this formula
  within 0.05% at 290-390 MeV/c in graphite);
- the small step of the Sternheimer delta at X = x0 for conductors (fitted separately in each range);
- energy-loss straggling: the energy loss is the mean, with no contribution to the q/p variance;
- the logarithmic term of Highland is applied to each layer separately. For a thick scatterer split into many thin
  layers, or for many thin layers in sequence, the sum of the per-layer variances is smaller than the variance of the
  whole thickness, because the log term is computed with the thin per-layer L/X0;
- charges other than ±1: the state carries only the sign of the charge.

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

The engine owns the magnetic field, the reconstruction calibration, the propagator settings and, optionally, the
material budget. With `"material" : "tracker_info"` it takes `geoinfo::tracker_info` from the current context at
construction (so `sand::geoinfo` and `sand::root_tgeomanager` must be configured in the job) and owns a
`station_material_budget` built from it; with `"none"` (the default) it has no material.

The engine is configured in the `"globals"` section of the job:

```json
"globals" : {
  "sand::tracker::kalman::engine" : {
    "field" : [0.6, 0.0, 0.0],
    "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0,
                      "tdc_resolution" : 3.5, "t0_offset" : 0.0 },
    "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 },
    "material" : "tracker_info"
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
| `material`                   | `string`    |       | `"none"`         | `"none"` or `"tracker_info"`.                   |

`calibration` and `propagator` are optional, and so is each of their keys; the defaults are those of
`reco_calibration` and `propagator_config`. Velocities, resolution, step and slope must be positive. Unknown keys are
rejected, so a misspelt key is an error and not a silent default; so is a `material` other than `"none"` and
`"tracker_info"`.

The field is uniform for now. It is owned by the engine through a pointer to the `field` interface, so the field map
read from the GDML will replace it without changing the modules.

A process gets the engine with `instance<sand::tracker::kalman::engine>()` and links `sand_tracker_kalman` in its
CMakeLists. The engine exposes

- `field()`, `calibration()`, `propagator_settings()`, `material()`: the shared state (`material()` is null with
  `"none"`);
- `propagate(state, z_target, mass)`: `propagate` with the field, settings and material of the engine (the
  field-only `propagate` with `"none"`);
- `propagate(state, z_target, mass, material)`: the material overload of `propagate`, with the field and settings of
  the engine and the material budget given by the caller instead of the engine's;
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

auto prop = kf.propagate(current, w.z(), 105.66); // material of the engine ("material" : "tracker_info")
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
- `kalman_material`: Highland and Bethe-Bloch reference values (Bethe-Bloch against the PDG table), density effect
  parametrisation, process noise, interval convention of the material budget, round trip with layers on the end
  planes, direction of the energy loss, stopping particle, empty budget identical to the field-only propagation,
  Jacobian with field and material against finite differences, pulls of a toy Monte Carlo with scattering and
  energy loss, `engine.propagate` with material identical to the free function with the field and settings of the
  engine;
- `kalman_filter`: closest approach distance, derivatives of the time model against finite differences, pulls of
  the fitted parameters on toy tracks;
- `kalman_engine`: engine built from a JSON configuration, defaults and configured values, rejection of invalid
  configurations, `engine.propagate` and `engine.wire_time` identical to the free functions with the same field,
  settings and calibration;
- `kalman_station_material`: Sternheimer rescaling (porous graphite to 2.21 g/cm3, delta shifted in X in all three
  ranges, identity and round trip), conversion of a geoinfo layer, the `material` key of the engine, and the
  transverse extent of a layer tested on the curved track: a 1 GeV/c muon in 0.6 T that enters the extent only because
  of the curvature gets the material, one that leaves it does not, and the round trip regains the energy.

The tests link `sand_tracker_kalman`.

The framework tests `510_kalman_station_material_drift`, `511_kalman_station_material_stt` and
`512_kalman_station_material_generic_drift` (process `sand::test::kalman_station_material` in
`tests/framework/510_modules`) build the engine with `"material" : "tracker_info"` on the test geometries and check
that a straight track through the centre of the tracker crosses every layer with a total x/X0 equal to the sum of
`station::radiation_lengths()`; that the backward propagations cross the same layers in reverse order; that a
propagation split at every layer centre crosses each layer once; that every layer carries the extent of its station
and a track outside the stations loses no energy and does not scatter; and that `engine.propagate` is identical to
`propagate` with an equivalent `fixed_material_budget`. 510 and 512 also fit 200 toy muons through the tracker
material (filter and smoother, x and y measured after each station) and check the pulls; 511 does not, because of
the propagation time through the STT radiators.

`gun_propagation_check.cpp` is not run by ctest. It compares the propagator with edep-sim trajectories of a particle
gun and needs an edep-sim file; build it against `sand_tracker_kalman` (include path `src/data/tracker`) and the
edep-sim I/O library.

## Validation so far

- Propagator against edep-sim truth (1 GeV μ⁻ in the tracker): the mean deviation is a few μm with the field on,
  against about 1 mm with the field off. The spread, about 0.2 mm, is multiple scattering. The time of flight agrees
  within 0.1 ps. This comparison predates the material model and used the field-only propagation.
- Toy fits: pull means within ±0.12 and widths between 0.99 and 1.09 for all parameters.
- Material toy (2000 muons at 1 GeV/c, ten 4 mm graphite layers, 0.6 T): pulls of x, y, tx, ty at the end with means
  within ±0.03 and widths between 0.96 and 1.00.
