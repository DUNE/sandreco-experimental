# Material description of the tracker stations

`sand::geoinfo::tracker_info` describes, for every tracker station, the material that a track crosses going along z:
the target, the chamber walls, the gas, and whatever else the geometry puts inside the station. This is the input for
material effects in track fitting (multiple scattering, energy loss). geoinfo does not depend on any fitting code:
an adapter converts these layers into the description a fitter needs.

## Data

Each `tracker_info::station` has `layers`, a `std::vector<tracker_info::material_layer>` ordered by z, with no
overlaps. A layer has:

| Member | Unit | Content |
|---|---|---|
| `z_min`, `z_max` | mm | upstream and downstream faces, global z |
| `radiation_length` | mm | X0 of the material |
| `density` | g/cm3 | density |
| `z_over_a` | mol/g | <Z/A> = sum_i w_i Z_i / A_i over the elements, w_i mass fractions |
| `mean_excitation` | MeV | mean excitation energy I |
| `material` | | name of the material in the ROOT geometry |
| `sternheimer` | | `std::optional<sternheimer_parameters>`: a, k, x0, x1, C, delta0 and the density they refer to; empty when unknown |

`thickness()` and `x_over_x0()` give the layer thickness in mm and in units of X0; `station::radiation_lengths()` is the
sum of x/X0 over the station.

The station also keeps `target_box` (full size of the target along global x, y, z, in mm, all positive) and
`target_density` (g/cm3). `tracker_info::target_masses()` returns kg.

## How the layers are built

While each station is built (`drift_info`, `generic_drift_info`, `stt_info`), `tracker_info::sample_material_layers`
takes a navigator from `root_tgeomanager`, places it on the upstream face of the station box at one point (x, y), and
steps along +z with `FindNextBoundaryAndStep` up to the downstream face:

- each step in a volume whose path is inside the station gives a layer; consecutive steps in the same volume are
  merged, while different volumes stay separate layers even when they are made of the same material (an STT target
  and the first radiator foil are both C3H6);
- steps in volumes of other stations (the station boxes overlap by a fraction of a micron) and steps shorter than
  1 pm are dropped.

The method does not depend on volume names, so it works for any chamber layout.

The sampling point is:

- for the drift chambers (`drift_info`, `generic_drift_info`), the centre (x, y) of the station; the drift planes
  are uniform in x and y, so any point gives the same layers;
- for the STT, a point halfway between two adjacent wires in each view (x between vertical wires, y between
  horizontal wires, taking the wires of all planes of the view so that staggered layers are included), at the gap
  closest to the station centre. Each straw has its wire on the axis, and the centre of the station can be on a
  straw axis: the line would then cross a wire head-on (40 um of gold, 0.012 X0, as much as a graphite target),
  which a track does only rarely. Between two wires the line crosses straw walls and gas only.

## Units of the geometry

The edep-sim geometries used by sandreco store lengths (and X0) in mm and densities in Geant4 internal units, where
1 g/cm3 = 6.24150907e18 (`tracker_material::geant4_g_per_cm3`). For example, the graphite of the targets has
`GetDensity()` = 1.392e19, that is 2.23 g/cm3, and X0 = 191.5 mm, i.e. rho X0 = 42.70 g/cm2, the value of the Geant4 G4_GRAPHITE material.
A target density outside 0.1-30 g/cm3 after the conversion stops the construction, since it means the geometry is in
other units.

## Material properties

`tracker_material` (`src/data/common/geoinfo/tracker_material.hpp`) computes the properties of a `TGeoMaterial`:

- density and X0 from the material, <Z/A> from its components (the isotopes are listed as separate components; this
  does not change the sums);
- I and the Sternheimer parameters from a table keyed by the material name in the geometry, if the material is there;
- otherwise I from the Bragg additivity rule,
  ln I = sum_i w_i (Z_i/A_i) ln I_i / sum_i w_i (Z_i/A_i), with elemental I_i from a table of the elements found in
  the SAND geometries (an element missing from that table is an error), and no density effect.

The material table:

| Geometry name | I [eV] | a | k | x0 | x1 | C | delta0 | rho_ref [g/cm3] |
|---|---|---|---|---|---|---|---|---|
| `Graphite` | 78.0 | 0.26142 | 2.8697 | -0.0178 | 2.3415 | 2.8680 | 0.12 | 2.265 |
| `C3H6` | 56.5 | 0.15045 | 3.2855 | 0.1534 | 2.4822 | 3.1252 | 0 | 0.900 |
| `Mylar` | 78.7 | 0.12679 | 3.3076 | 0.1562 | 2.6507 | 3.3262 | 0 | 1.400 |

Sources, all as tabulated in Geant4 10.6.3:

- I of the materials and of the elements: ICRU Report 37 (1984), `G4NistMaterialBuilder` and
  `G4NistManager::GetMeanIonisationEnergy`;
- Sternheimer parameters: Sternheimer, Berger and Seltzer, At. Data Nucl. Data Tables 30 (1984) 261,
  `G4DensityEffectData` entries of `G4_GRAPHITE` (index 276), `G4_POLYPROPYLENE` (222) and `G4_MYLAR` (219), unchanged.

rho_ref is the density the parameters were computed at, from the plasma energy of the entry,
hbar omega_p = 28.816 eV sqrt(rho <Z/A>). For graphite it is 2.265 g/cm3, although Geant4 builds `G4_GRAPHITE` at
2.210 g/cm3.

The Sternheimer parameters are given at the table density rho_ref, not at the density of the geometry (graphite 2.23,
C3H6 0.946 g/cm3): any density correction is left to the user of the layers.

For carbon the Bragg rule uses I = 81 eV (the Geant4 element value), while the `Graphite` entry has I = 78 eV.

The chamber gases are Ar/CO2 mixtures with no tabulated entry: `driftGas` (85/15 by mass, I = 166.9 eV from the Bragg
rule), `stGas_Ar19` and `stGas_Xe19` (both 70/30 Ar/CO2 by mass despite the name, I = 148.8 eV). They have no density
effect, which in a gas only matters above beta gamma of about 50.

## Limitations

- The layers describe the material along one line. This is exact for the drift chambers, whose planes are uniform in
  x and y, but not for the STT: there the line crosses one particular sequence of straw walls and gas (between the
  wires, see above), not an average over the straw plane. An averaged description of the straw planes is a follow-up.
- The STT C3H6 stations contain a transition radiator of about 100 C3H6 foils with air gaps: each foil and each gap is a
  layer (about 230 layers per station).
- No energy-loss straggling parameters are provided.
