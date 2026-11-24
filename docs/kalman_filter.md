# Kalman filter library for the tracker

The complex data module `sand_tracker_kalman`, in `src/data/tracker/kalman/`, contains the math of the track fit for
the SAND tracker: the track state, its propagation through the magnetic field and the material, the measurement
models for a single wire, the Kalman update and the smoother. Nothing in it is specific to one tracker technology: the
measurement model applies to drift chambers and straw tubes alike, and is the same model used by the fast
digitizations `drift_fast_digi` and `stt_fast_digi`.

Modules link `sand_tracker_kalman` and include the headers as `<kalman/state.hpp>`, `<kalman/propagator.hpp>`,
`<kalman/material.hpp>`, `<kalman/measurement.hpp>`, `<kalman/filter.hpp>`, `<kalman/daf.hpp>`, `<kalman/field.hpp>`
and `<kalman/engine.hpp>`. The
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
(see "Material" below). For each layer, in order, it propagates in the field to the layer centre, changes q/p by the
energy loss and adds the scattering noise `Q`; then it propagates to `z_target`. The covariance is accumulated step by
step as `C <- F_k C F_k^T + Q_k`, so the returned covariance includes the noise, while the returned `F` is the
product of all the Jacobians (field and material steps) without noise, which is what the smoother expects. With no
layers in the interval the result is identical to the field-only overload.

Both overloads return nothing if the track becomes steeper than `propagator_config::max_slope`, which happens for
tracks that curl back in the field; the material overload also returns nothing if the particle stops in a layer.

## Material (`material.hpp`)

Material is a sequence of thin slabs perpendicular to z. A `layer` has its centre `z` and `thickness` along z [mm],
the radiation length `radiation_length` [mm], `density` [g/cm3], `z_over_a` [mol/g], `mean_excitation` I [MeV] and
`sternheimer`, the density effect parameters (`density_effect`: a, k, x0, x1, C-bar, delta0, as in the PDG tables of
atomic and nuclear properties). Left at its default, `sternheimer` disables the density effect correction.

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

- `fixed_material_budget`: a fixed list of layers, independent of x and y. Used in the tests.
- planned, outside this module: one built from the tracker stations of `tracker_info` (targets and chamber walls
  as layers at fixed z), and one stepping through the ROOT geometry along the track.

The engine does not own a material budget yet: `engine.propagate(state, z_target, mass, material)` takes it from the
caller and uses the field and the propagator settings of the engine. There are no configuration keys for the
material. The budget built from `tracker_info` will come with a later change, which will also decide who owns it.

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

## Deterministic annealing filter (`daf.hpp`)

`daf_fit` fits a track when the assignment of the measurements is ambiguous: the two sides of each wire, and other
digits on the same plane. It is the deterministic annealing filter of R. Frühwirth and A. Strandlie, Comput. Phys.
Commun. 120 (1999) 197: a Kalman filter and smoother iterated with weights on the competing measurements, annealed
from a high temperature down to T = 1, with the measurement variance annealed as well (as in GENFIT).

**The seed must give the track time to a few ns.** The time is a parameter of the fit and, while the sides are not
known, it is nearly degenerate with the drift distances: see "Seed requirements" below.

### Sites and candidates

The input is a list of `site`s, ordered along the motion of the particle (z increasing for `direction = +1`,
decreasing for `direction = -1`). A site is a wire plane (`z`) with its `measurement_candidate`s: each is a wire, a
measured TDC and a side (+1 or -1), plus an opaque `id` that the fit does not use (the module puts the index of the
digit there). A wire digit gives two candidates, one per side; other digits on the same plane are more candidates.
The wires of a site must lie on its plane (within 1 μm). The sites are built from the digits by the reconstruction
module; that, the track finder and the competition of several tracks for the same digits are outside this library.

### Algorithm

With `V = tdc_resolution^2`, there is one pass for each temperature `T` of the schedule:

1. weights at `T` from the smoothed states of the previous pass (the first pass uses the initial weights, see
   below). For candidate `i`, with `r_i` the residual of `wire_time` on the smoothed state:

   ```
   chi2_i = r_i^2 / (T V)
   phi_i  = exp(-chi2_i / 2)
   w_i    = phi_i / (sum_j phi_j + exp(-chi2_cut / (2T)))
   ```

   The sum runs over the candidates of the same site. The cutoff term is the weight left to "none of them": a
   single candidate at `r^2 / V = chi2_cut` gets weight 1/2 at any temperature. The weights are computed with a
   common scale factor, so that candidates far above the cutoff get weight 0 instead of 0 / 0 (`daf_weights`);
2. forward filter: propagate site by site, with the material; at each site update with every candidate whose weight
   is above `min_weight`, one after the other, with `update(..., weight)` and measurement variance `T V`, i.e.
   `T V / w`. With `anneal_variance = false` the variance is `V / w` at every temperature (the weights are the
   same);
3. Rauch-Tung-Striebel smoother over the pass.

After the schedule, passes at `T = 1` are repeated until the weights computed from the last pass differ from those
it used by no more than `convergence_threshold`, or `max_iterations` extra passes are done; `converged` says which.

Final pass: then one more forward filter and smoother at `T = 1`, with those weights fixed and the covariance of the
prior multiplied by `final_prior_scale` (default 10^4). The states, residuals, `chi2` and `ndf` of the result come
from this pass. The reason: in the reconstruction the seed is built by the track finder from the same digits that the
fit uses, so it is not an independent measurement. Keeping its covariance would count those digits twice, make the
covariance of the result too small and give `chi2/ndf` above 1 (about `sum w / (sum w - 6)`, 1.23 in the toy).
Inflating it keeps the seed only as the starting point of the linearisation (which in the final pass is the converged
track anyway), and `chi2` has mean `ndf`. With `final_prior_scale = 1` there is no final pass, and the result is the
last pass of the annealing, with the seed counted as a measurement: use it when the seed is independent of the digits
(for example a vertex or a track from another detector).

The default schedule is `{81, 81, 9, 4, 1, 1}`. The first temperature is repeated because the first pass uses the
initial weights, not weights computed at 81 from a track: without the second pass at 81 the toy loses 1-2% of
left-right efficiency. With the variance annealed, the first passes are soft fits (σ = 9 × 3.5 ns at T = 81, about
1.6 mm in drift distance) through the wire positions, which the following temperatures sharpen. Without the variance
annealing the first pass forces the track onto the wires (see "Seed requirements").

Choices:

- Initial weights (`initial_weights`): `equal` (default) gives `1/n` to the n candidates of each site, so that every
  site starts with the information of one measurement (0.5 and 0.5 for the two sides of a wire). `prediction`
  propagates the prior to each site without updates and computes the weights at the first temperature with
  `r_i^2 / (T V + H C H^T)`, `C` the covariance of the prediction, so that sites far from the prior get nearly equal
  weights. In the toy, `prediction` loses about 1% of left-right efficiency both with a 5 mm, 10 ns seed and with a
  1 mm, 2 ns seed, so the default is `equal`.
- Linearisation: in the first pass the propagation and the measurements are linearised at the predicted state, as in
  an extended Kalman filter. In the following passes they are linearised around the smoothed track of the previous
  pass: the propagation Jacobian, the noise and the energy loss are those of the reference track, and
  `h(x) = h(x_ref) + H (x - x_ref)`. The prior (the initial state and its covariance) stays the same in every pass.
  With a single candidate per site, `T = {1}`, equal initial weights and weights that stay at 1, the result is the
  plain filter and smoother.
- The candidates of a site are all linearised at the same point, so the order in which they are applied does not
  change the result.
- A site where no candidate is above `min_weight` is crossed without update and listed in `skipped_sites` (for the
  last pass). Its weights are still recomputed after the pass, so it can come back.

### Result

`daf_result` holds the smoothed state on each site, the weight of each candidate (`weights[site][candidate]`) and its
residual on the smoothed state (`residuals[site][candidate]`, in ns), the number of passes (the final pass included)
and the convergence flag, and

- `chi2`: the filter chi2 of the final pass (of the last pass if there is none), `sum_i w_i r_i^2 / (V + H C H^T)`
  over the candidates used, with `r_i` the residual and `C` the covariance before the update of that candidate, and
  `V` not scaled by the temperature;
- `ndf = sum_i w_i - 6`, all the weights minus the six track parameters;
- `smoothed_chi2`: `sum_i w_i r_i^2 / V` with the smoothed residuals.

In the toy with the informative seed of the tests (true sides given, weights 1), `chi2/ndf` has mean 1.00 with the
final pass (1000 tracks) against 1.25 without it; with a seed that is diffuse to begin with it is 0.96-1.03 either
way. With the sides competing it is 0.91-0.98: sites close to the wire, where the two sides share the weight, count
as one measurement in `ndf` and contribute little to `chi2`. `smoothed_chi2` is smaller, about 0.8 `ndf`: the
smoothed track follows part of the scattering.

Failures do not throw. `success` is false and `status` and `reason` say why:

| `status` | When |
|----------|------|
| `invalid_config` | the `daf_config` is not valid (`check(cfg)` explains why) |
| `invalid_input` | no sites, a site without candidates, a side other than ±1, a wire off the plane of its site, sites not strictly ordered along the motion |
| `propagation_failed` | a propagation failed (track too steep, particle stopped in the material), in a pass or when propagating the prior for the initial weights |
| `not_positive_definite` | the initial, a filtered or a smoothed covariance is not positive definite, or the smoother could not invert a predicted covariance |
| `no_measurements` | in a pass every candidate of every site was below `min_weight` |

`reason` gives the pass and the site.

### Configuration

`daf_config`, and the `"daf"` section of the engine configuration:

| Key | Default | Meaning |
|-----|---------|---------|
| `temperatures` | [81, 81, 9, 4, 1, 1] | annealing schedule, one pass per temperature, all positive |
| `chi2_cut` | 9 | cutoff chi2 (3 sigma), positive |
| `max_iterations` | 10 | maximum number of extra passes at T = 1, non-negative integer |
| `convergence_threshold` | 0.001 | converged when no weight changes by more than this, positive |
| `min_weight` | 0.001 | candidates with a weight not above this are left out of the update, in [0, 1) |
| `anneal_variance` | true | update with variance `T V / w`; false: `V / w` |
| `initial_weights` | "equal" | weights of the first pass: "equal" or "prediction" |
| `final_prior_scale` | 1e4 | factor on the prior covariance in the final pass, at least 1; 1: no final pass |

### Seed requirements

With unknown sides, the two sides of a wire at similar weights act together as a measurement of the time at the wire
(`t + L/v_wire = TDC`, late by the drift time) and of a track through the wire (`d = 0`). A shift of the track time
moves all the drift distances together, so if the seed leaves the time free by more than a few ns the annealing can
settle on a late time with the track pulled towards the wires, and the sides wrong. The position of the seed matters
much less. Fraction of sites where the true side gets weight above 0.5, by drift distance, in the toy described in
"Validation" (500 forward tracks per line, default settings), with the pull of the time on the first site and the
RMS of the q/p pull:

| σ(x, y) | σ(tx, ty) | σ(t) | 0.5-1 mm | 1-2 mm | 2-3 mm | 3-5 mm | t pull mean | q/p pull RMS |
|---------|-----------|------|----------|--------|--------|--------|-------------|--------------|
| 10 mm   | 0.05      | 20 ns | 77%   | 77%   | 83%   | 84%   | 5.9  | 1.36 |
| 5 mm    | 0.05      | 10 ns | 97.1% | 96.7% | 98.0% | 98.2% | 0.78 | 1.04 |
| 1 mm    | 0.01      | 10 ns | 96.2% | 97.1% | 97.7% | 98.3% | 0.68 | 1.02 |
| 2 mm    | 0.02      | 5 ns  | 98.7% | 99.3% | 99.4% | 99.3% | 0.30 | 1.00 |
| 5 mm    | 0.05      | 2 ns  | 99.2% | 99.6% | 99.6% | 99.5% | 0.24 | 1.00 |
| 2 mm    | 0.02      | 2 ns  | 99.0% | 99.5% | 99.5% | 99.5% | 0.24 | 1.00 |
| 1 mm    | 0.01      | 2 ns  | 98.7% | 99.4% | 99.6% | 99.6% | 0.24 | 1.00 |
| 0.5 mm  | 0.005     | 1 ns  | 99.1% | 99.5% | 99.6% | 99.7% | 0.24 | 1.00 |

σ(q/p) is 25% of q/p in the lines with 0.05 on the slopes and 10% in the others. With the true side given, the same
fit reaches 99.85%: a true hit with a 3 sigma fluctuation has its chi2 above the cutoff and weight below 0.5 by
construction. Without the variance annealing the 5 mm, 10 ns seed gives 70-80%; with the 1 mm, 2 ns seed the variance
annealing makes no difference to the efficiency (98.8-99.8% without it).

The track finder must therefore provide the time to about 2-5 ns (from the ECAL, or from a fit of the drift times
of the seed hits with their sides).

### Known limitations

- Sites close to the wire, where the two sides keep similar weights, pull the time late by about `|d| / v_drift`:
  the time pull has a mean of +0.2 to +0.3 (0.15-0.22 ns) with a good seed.
- With small stereo angles, x is measured through the stereo views only; flipping the sides in a few u and v planes
  and moving x is a nearby local minimum. A few tracks in a thousand end in one; they give the tails of the y and ty
  pulls (0.4-0.8% beyond 5).
- The covariance of the result does not include the uncertainty of the assignment.
- A digit far outside the drift window (hundreds of ns) on a site with few candidates distorts the first pass, which
  gives it weight 1/n; the module building the sites should not offer such digits.

## Engine (`engine.hpp`)

`sand::tracker::kalman::engine` is the complex data object of the library. The seeder, the combinatorial track
finder, the fit and smoother and the vertex fit all need the same state: magnetic field, material description and
reconstruction calibration. Keeping that state in one global, unique complex data object means it is configured once
per job and every module sees the same instance; and since the library is compiled once into `sand_tracker_kalman`,
every module runs the same code, instead of each one compiling its own copy of header-only functions (which
duplicates code and can give slightly different numerical results).

The engine owns the magnetic field, the reconstruction calibration, the propagator settings and the settings of the
deterministic annealing filter. The material
description will be added to it when it comes from `tracker_info`, in a later merge request; until then the caller
passes a `material_budget` to `engine.propagate(state, z_target, mass, material)`.

The engine is configured in the `"globals"` section of the job:

```json
"globals" : {
  "sand::tracker::kalman::engine" : {
    "field" : [0.6, 0.0, 0.0],
    "calibration" : { "drift_velocity" : 0.05, "wire_velocity" : 200.0,
                      "tdc_resolution" : 3.5, "t0_offset" : 0.0 },
    "propagator" : { "max_step" : 50.0, "max_slope" : 1000.0 },
    "daf" : { "temperatures" : [81, 81, 9, 4, 1, 1], "chi2_cut" : 9.0, "max_iterations" : 10,
              "convergence_threshold" : 0.001, "min_weight" : 0.001, "anneal_variance" : true,
              "initial_weights" : "equal", "final_prior_scale" : 1e4 }
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
| `daf.temperatures`           | `double[]`  |       | [81, 81, 9, 4, 1, 1] | Annealing schedule of the DAF.              |
| `daf.chi2_cut`               | `double`    |       | 9                | Cutoff chi2 of the DAF weights.                 |
| `daf.max_iterations`         | `int`       |       | 10               | Maximum extra DAF passes at T = 1.              |
| `daf.convergence_threshold`  | `double`    |       | 0.001            | Largest change of a weight at convergence.      |
| `daf.min_weight`             | `double`    |       | 0.001            | Candidates not above it are left out.           |
| `daf.anneal_variance`        | `bool`      |       | true             | Update with variance T V / w (false: V / w).    |
| `daf.initial_weights`        | `string`    |       | "equal"          | First-pass weights: "equal" or "prediction".    |
| `daf.final_prior_scale`      | `double`    |       | 1e4              | Prior covariance factor of the final pass.      |

`calibration`, `propagator` and `daf` are optional, and so is each of their keys; the defaults are those of
`reco_calibration`, `propagator_config` and `daf_config`. Velocities, resolution, step, slope, temperatures, cutoff
and convergence threshold must be positive, `max_iterations` a non-negative integer, `min_weight` in [0, 1),
`anneal_variance` a boolean, `initial_weights` "equal" or "prediction" and `final_prior_scale` at least 1.
Unknown keys are rejected, so a misspelt key is an error and not a silent default.

The field is uniform for now. It is owned by the engine through a pointer to the `field` interface, so the field map
read from the GDML will replace it without changing the modules.

A process gets the engine with `instance<sand::tracker::kalman::engine>()` and links `sand_tracker_kalman` in its
CMakeLists. The engine exposes

- `field()`, `calibration()`, `propagator_settings()`, `daf_settings()`: the shared state;
- `propagate(state, z_target, mass)`: `propagate` with the field and settings of the engine;
- `propagate(state, z_target, mass, material)`: the material overload of `propagate`, with the field and settings of
  the engine and the material budget given by the caller;
- `wire_time(state, wire, side)`: `wire_time` with the calibration of the engine;
- `fit(initial, sites, mass, material)`: `daf_fit` with the field, calibration, propagator and DAF settings of the
  engine and the material budget given by the caller.

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

fixed_material_budget const material(layers); // or a budget built from the geometry

auto prop = kf.propagate(current, w.z(), 105.66, material);
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
- `kalman_daf`: the weight formula; with one candidate per site and `T = {1}` the DAF equals the plain filter and
  smoother (both initial-weight options); left-right resolution, pulls and chi2/ndf on toy tracks moving along +z and
  -z, with the wires oriented as in `drift_info`; noise digits; chi2/ndf with and without the final pass; an outlier;
  failure statuses; independence of the order of the candidates; prior on another plane; `engine.fit` identical to
  `daf_fit` with the settings of the engine, and the `"daf"` configuration keys read and validated;
- `kalman_engine`: engine built from a JSON configuration, defaults and configured values, rejection of invalid
  configurations, `engine.propagate` and `engine.wire_time` identical to the free functions with the same field,
  settings and calibration.

The tests link `sand_tracker_kalman`.

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
- DAF toy (`kalman_daf`). Detector: 30 wire planes 50 mm apart, 10 mm pitch, a 4 mm graphite layer every three
  planes, 0.6 T along x, calibration defaults (3.5 ns TDC resolution, 0.05 mm/ns drift velocity). The wires are
  oriented as in `sand::geoinfo::drift_info`: in the 0 degree view (X) they run along x and measure y, the bending
  coordinate; the U and V views are rotated by -5 and +5 degrees; the readout end is at smaller x. Tracks: 1 GeV/c
  muons generated with the library, with random scattering at each layer; on each plane the closest wire, its TDC
  smeared by the resolution. Prior: the true state on the first site smeared with, and carrying, the covariance
  σ(x, y) = 1 mm, σ(tx, ty) = 0.01, σ(q/p) = 10%, σ(t) = 2 ns. Default DAF settings (final pass included), both
  sides of each wire as candidates; 250 forward, 200 backward and 250 noise tracks (the numbers of the test; the seed
  table above uses 500).

  Fraction of sites where the true side gets weight above 0.5, by drift distance:

  | Drift [mm] | 0-0.1 | 0.1-0.25 | 0.25-0.5 | 0.5-1 | 1-2 | 2-3 | 3-5 |
  |------------|-------|----------|----------|-------|-----|-----|-----|
  | forward    | 52%   | 80%      | 94%      | 98.9% | 99.8% | 99.7% | 99.5% |
  | backward   | 56%   | 81%      | 98%      | 99.5% | 99.8% | 99.7% | 99.3% |
  | 20% noise  | 45%   | 73%      | 94%      | 97.0% | 97.1% | 98.0% | 98.5% |

  Pulls of the smoothed parameters on the first site (core width: RMS of the pulls within ±4; the statistical error
  of the means is about 0.065):

  | | x | y | tx | ty | q/p | t |
  |-|---|---|----|----|-----|---|
  | forward: mean       | -0.01 | 0.08 | 0.04 | -0.11 | 0.06 | 0.22 |
  | forward: core width | 0.92  | 1.00 | 0.98 | 1.02  | 0.96 | 0.98 |
  | forward: RMS        | 0.92  | 1.12 | 0.98 | 1.02  | 0.96 | 0.98 |
  | backward: mean       | 0.04 | -0.01 | 0.03 | -0.01 | -0.13 | 0.18 |
  | backward: core width | 0.92 | 1.02  | 0.99 | 1.03  | 1.03  | 1.02 |
  | backward: RMS        | 0.92 | 1.02  | 0.99 | 1.03  | 1.03  | 1.19 |
  | noise: mean       | -0.02 | -0.03 | -0.06 | -0.01 | -0.03 | 0.31 |
  | noise: core width | 0.98  | 1.15  | 0.95  | 1.09  | 1.05  | 1.10 |
  | noise: RMS        | 0.98  | 1.44  | 1.09  | 1.18  | 1.05  | 1.10 |

  With 1000 forward tracks the core widths are 0.98-1.03 and the means within ±0.04, except the time (+0.19).
  Time on the first site, fitted minus true: +0.16 ns (forward), +0.14 ns (backward), +0.22 ns (noise), for an RMS of
  0.76-0.99 ns. chi2/ndf has mean 0.91 (forward), 0.93 (backward), 0.98 (noise); with the smoothed residuals 0.79-0.84.
  Noise (another wire one or two cells away, random TDC in the drift window, on 20% of the planes): mean weight of
  the noise candidates 0.0009. The fits take 9.7-10.4 passes on average (final pass included); 97% (forward), 94.5%
  (backward) and 94% (noise) converge within the 10 extra passes. A TDC shifted by 20 sigma gets weight below 1e-61
  on both sides in 50 tracks out of 50, and the tracks are fitted.
