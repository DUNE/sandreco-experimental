#pragma once

#include <kalman/state.hpp>

#include <optional>
#include <vector>

/**
 * @file material.hpp
 * @brief Material effects for the tracker Kalman filter: thin layers, the material budget interface, multiple
 * scattering (Highland) and mean energy loss (Bethe-Bloch).
 *
 * Material is described as a sequence of thin slabs perpendicular to z. The library does not know where the
 * material comes from: the reconstruction provides an implementation of material_budget, in the same way as it
 * provides a field. See docs/kalman_filter.md, section "Material".
 *
 * All particles are assumed to have unit charge in the material step, since the state carries only the sign of the
 * charge in q/p.
 */

namespace sand::tracker::kalman {

  inline constexpr double s_electron_mass = 0.51099895; ///< Electron mass [MeV]
  inline constexpr double s_bethe_k       = 0.307075;   ///< K = 4 pi N_A r_e^2 m_e c^2 [MeV cm2/mol]
  inline constexpr double s_highland_e0   = 13.6;       ///< Highland constant [MeV]

  /**
   * @brief Sternheimer parameters of the density effect correction delta, as tabulated by the PDG for each material
   * (Sternheimer, Berger and Seltzer, Atomic Data and Nuclear Data Tables 30 (1984) 261).
   *
   * With X = log10(beta gamma):
   *  - X >= x1      : delta = 2 ln(10) X - c_bar
   *  - x0 <= X < x1 : delta = 2 ln(10) X - c_bar + a (x1 - X)^k
   *  - X < x0       : delta = delta0 10^(2 (X - x0))   (zero for insulators, delta0 = 0)
   *
   * The default (all zero) means no density effect correction.
   */
  struct density_effect {
    double a{0};      ///< Parameter a
    double k{0};      ///< Exponent k (m_s in the PDG tables)
    double x0{0};     ///< Lower limit x0 of X = log10(beta gamma)
    double x1{0};     ///< Upper limit x1 of X = log10(beta gamma)
    double c_bar{0};  ///< Parameter C-bar; zero disables the correction
    double delta0{0}; ///< Value of delta at x0 for conductors, zero for insulators

    /// Whether the parameters describe a correction (c_bar is non-zero for every tabulated material)
    bool enabled() const { return c_bar != 0.; }
  };

  /// A thin slab of homogeneous material, perpendicular to the z axis.
  struct layer {
    double z{0};                  ///< Centre of the slab [mm]
    double thickness{0};          ///< Extent along z [mm]
    double radiation_length{0};   ///< Radiation length X0 [mm]
    double density{0};            ///< Density [g/cm3]
    double z_over_a{0};           ///< Ratio <Z/A> [mol/g]
    double mean_excitation{0};    ///< Mean excitation energy I [MeV]
    density_effect sternheimer{}; ///< Density effect parameters; default: no correction
  };

  /**
   * @brief Source of the material crossed by a track between two planes.
   *
   * Intended implementations: from the tracker stations of the geometry service, and by stepping through the ROOT
   * geometry. Neither is part of this library.
   */
  class material_budget {
   public:
    virtual ~material_budget();
    /**
     * @brief Layers crossed between the plane of s and z_target.
     * @param s        Starting state. Its position and direction are available to implementations that step through
     *                 the geometry.
     * @param z_target Target plane [mm].
     * @return The layers whose centre lies between s.z and z_target, ordered from s.z towards z_target (empty if
     *         z_target == s.z). The interval is half-open according to the direction of motion: with lo and hi the
     *         smaller and larger of s.z and z_target, it is (lo, hi] for s.direction = +1 and [lo, hi) for
     *         s.direction = -1 (see layer_in_interval()).
     *
     * A layer whose centre lies on a plane belongs to the segment that reaches that plane along the motion, so a
     * state on a plane is always physically downstream of the material on it, whichever way it was propagated. When
     * propagating along the motion (forward filter) a layer on the starting plane is excluded and one on the target
     * plane included; against the motion (backward fit) it is the other way round. A propagation and its reverse
     * therefore cross the same layers, and no layer is counted twice.
     */
    virtual std::vector<layer> crossed_layers(const state& s, double z_target) const = 0;
  };

  /**
   * @brief Whether a layer centre belongs to the propagation from z_from to z_to, with the convention of
   * material_budget::crossed_layers().
   * @param z_layer   Layer centre [mm]
   * @param z_from    Starting plane [mm]
   * @param z_to      Target plane [mm]
   * @param direction Sign of p_z of the particle
   * @return true if z_layer is in (lo, hi] for direction = +1, in [lo, hi) for direction = -1, with lo and hi the
   *         smaller and larger of z_from and z_to; false if z_from == z_to.
   */
  bool layer_in_interval(double z_layer, double z_from, double z_to, int direction);

  /// A fixed list of layers, independent of x and y. Used in tests and for simple setups.
  class fixed_material_budget : public material_budget {
   public:
    /// @param layers The layers, in any order.
    explicit fixed_material_budget(std::vector<layer> layers);

    /// See material_budget::crossed_layers. x and y of s are ignored.
    std::vector<layer> crossed_layers(const state& s, double z_target) const override;

   private:
    std::vector<layer> m_layers; ///< Sorted by increasing z
  };

  /**
   * @brief Path length of a straight track through a layer.
   * @param l      The layer
   * @param tx, ty Slopes of the track
   * @return L = thickness * sqrt(1 + tx^2 + ty^2) [mm]
   */
  double layer_path_length(const layer& l, double tx, double ty);

  /**
   * @brief RMS of the projected multiple scattering angle, Highland formula.
   * @param p      Momentum [MeV]
   * @param mass   Mass hypothesis [MeV]
   * @param length Path length in the material [mm]
   * @param x0     Radiation length [mm]
   * @param charge Charge [e]
   * @return theta0 = 13.6 MeV / (beta p) * |q| * sqrt(L/X0) * (1 + 0.038 ln(L q^2 / (X0 beta^2))) [rad], clamped at
   *         zero for the (unphysically) thin layers where the bracket becomes negative.
   */
  double highland_theta0(double p, double mass, double length, double x0, double charge = 1.);

  /**
   * @brief Process noise of a thin scatterer on the slopes.
   * @param theta0 RMS projected scattering angle [rad]
   * @param tx, ty Slopes at the scatterer
   * @return Q with, for N^2 = 1 + tx^2 + ty^2, Q(tx,tx) = theta0^2 (1 + tx^2) N^2, Q(ty,ty) = theta0^2 (1 + ty^2) N^2,
   *         Q(tx,ty) = theta0^2 tx ty N^2 and all other elements zero.
   */
  sym_matrix_t scattering_noise(double theta0, double tx, double ty);

  /**
   * @brief Density effect correction delta of the Bethe-Bloch formula, Sternheimer parametrisation.
   * @param beta_gamma p / M of the particle
   * @param d          Sternheimer parameters of the material
   * @return delta (dimensionless); zero if the parameters are not set.
   *
   * The PDG parameters are fitted separately in each range of X, so delta can have a small step at X = x0 for
   * conductors (about 0.01 in graphite, i.e. 0.1% of dE/dx at beta gamma close to 1).
   */
  double density_effect_delta(double beta_gamma, const density_effect& d);

  /**
   * @brief Mean energy loss per unit length, Bethe-Bloch formula with the density effect correction.
   * @param p      Momentum [MeV]
   * @param mass   Mass hypothesis [MeV]
   * @param l      Material (density, Z/A, I and the Sternheimer parameters are used)
   * @param charge Charge [e]
   * @return dE/dx = K q^2 (Z/A) rho / beta^2 * [ 0.5 ln(2 me beta^2 gamma^2 Tmax / I^2) - beta^2 - delta/2 ] [MeV/mm],
   *         with Tmax = 2 me beta^2 gamma^2 / (1 + 2 gamma me/M + (me/M)^2) and delta from density_effect_delta().
   *
   * Without the Sternheimer parameters (layer::sternheimer left at its default) delta is zero: in graphite this
   * overestimates dE/dx by about 4% at 300 MeV/c, 10% at 1 GeV/c and 15% at 3 GeV/c for a muon. Shell corrections
   * and higher-order terms are neglected. Below the validity range of the formula the bracket can become negative;
   * the value is returned as is and the caller treats a non-positive result as a stopped particle.
   */
  double bethe_bloch(double p, double mass, const layer& l, double charge = 1.);

  /**
   * @brief Whether a propagation from z_from to z_to goes along the motion of the particle.
   * @param direction Sign of p_z of the particle
   * @param z_from    Starting plane [mm]
   * @param z_to      Target plane [mm]
   * @return +1 if sign(z_to - z_from) == direction (the particle loses energy), -1 otherwise (backward fit or
   *         smoother: the particle gains the energy it lost).
   */
  int energy_loss_sign(int direction, double z_from, double z_to);

  /// Output of material_step().
  struct material_effects {
    vector_t par;            ///< State parameters after the layer (only q/p changes)
    matrix_t F;              ///< Jacobian d(par out)/d(par in): identity except the q/p row
    sym_matrix_t noise;      ///< Process noise Q from multiple scattering
    double energy_change{0}; ///< E_out - E_in [MeV], negative for a loss
  };

  /**
   * @brief Effect of one thin layer on the state: energy loss on q/p and multiple scattering noise on the slopes.
   * @param par       State parameters at the layer
   * @param l         The layer
   * @param mass      Mass hypothesis [MeV]
   * @param loss_sign +1 to lose energy, -1 to gain it (see energy_loss_sign())
   * @return The new parameters, Jacobian and noise, or nothing if the particle stops in the layer.
   *
   * The energy after the layer solves E1 = E0 - loss_sign * L * dE/dx(Em) with Em = (E0 + E1) / 2 (implicit midpoint).
   * The scheme is symmetric: losing and then gaining the energy in the same layer returns the initial energy to the
   * precision of the iteration, so a forward and a backward propagation are exact inverses. The scattering angle is
   * also evaluated at the midpoint momentum. Unit charge is assumed. A track with q/p = 0 (infinite momentum) is
   * left unchanged and gets no noise.
   *
   * The q/p row of the Jacobian is analytic given dE/dx and its derivative with respect to the energy, which is
   * taken by central differences:
   *   dE1/dE0 = (1 - s L f'/2) / (1 + s L f'/2),  dE1/dL = -s f / (1 + s L f'/2),  f = dE/dx(Em), f' = df/dE,
   * then d(q/p)/dE = -q E / p^3 and dL/dtx = thickness * tx / N.
   */
  std::optional<material_effects> material_step(const vector_t& par, const layer& l, double mass, int loss_sign);

} // namespace sand::tracker::kalman
