#pragma once

#include <kalman/field.hpp>
#include <kalman/state.hpp>

#include <optional>

/**
 * @file propagator.hpp
 * @brief Runge-Kutta propagation of the track state in z, with the transport Jacobian.
 *
 * Equations of motion with z as independent variable, d = sign(p_z), N = sqrt(1 + tx^2 + ty^2), kappa =
 * CLHEP::c_light * CLHEP::tesla (field B in T, see state.hpp), c = CLHEP::c_light:
 *
 *   dx/dz  = tx
 *   dy/dz  = ty
 *   dtx/dz = d * kappa * (q/p) * N * ( tx*ty*Bx - (1 + tx^2)*By + ty*Bz )
 *   dty/dz = d * kappa * (q/p) * N * ( (1 + ty^2)*Bx - tx*ty*By - tx*Bz )
 *   dqop/dz = 0                                   (energy loss is added by the material model, not here)
 *   dt/dz  = d * N * sqrt(1 + m^2 (q/p)^2) / c    (time of flight for the mass hypothesis m)
 *
 * The Jacobian F = d(state at z1)/d(state at z0) is integrated together with the state with the same fourth-order
 * Runge-Kutta scheme, using dF/dz = A(s) F with A = df/ds. Derivatives of the field with respect to position are
 * neglected, which is exact for a uniform field.
 *
 * The derivation of these equations and of the Jacobian is in docs/kalman_filter.md, section "Propagation".
 */

namespace sand::tracker::kalman {

  /**
   * @brief Settings of the Runge-Kutta propagation.
   *
   * No default values: every tunable parameter comes from the configuration (keys `propagator.max_step` and
   * `propagator.max_slope` of the engine). Code that builds the struct directly sets both members.
   */
  struct propagator_config {
    double max_step;  ///< Maximum Runge-Kutta step [mm]
    double max_slope; ///< Propagation fails if |tx| or |ty| exceed this
  };

  /// Output of propagate().
  struct propagation_result {
    state out;             ///< State on the target plane, covariance transported as F C F^T
    matrix_t F;            ///< Transport Jacobian
    double path_length{0}; ///< Length travelled [mm]
  };

  namespace detail {

    /**
     * @brief Right-hand side f(s) and its Jacobian A = df/ds for the equations above.
     *
     * See docs/kalman_filter.md, section "Propagation".
     * @param s    State vector
     * @param d    Sign of p_z
     * @param mass Mass hypothesis [MeV]
     * @param b    Field at the position of s [T]
     * @param f    Output: ds/dz
     * @param A    Output: df/ds
     */
    void derivatives(const vector_t& s, int d, double mass, const field_vector& b, vector_t& f, matrix_t& A);

  } // namespace detail

  /**
   * @brief Propagate a state to the plane z_target.
   * @param in      Input state (its covariance is transported).
   * @param z_target Target plane [mm]. May be upstream or downstream of in.z.
   * @param mass    Mass hypothesis [MeV], used only for the time of flight.
   * @param bfield  Magnetic field.
   * @param cfg     Runge-Kutta settings (no default: from the configuration).
   * @return The propagated state and Jacobian, or nothing if the track becomes too steep.
   * @throws std::invalid_argument if cfg.max_step or cfg.max_slope is not positive (for example a value-initialised
   *         propagator_config).
   *
   * The direction of motion is a property of the particle (in.direction) and is not changed by propagating to a
   * plane on the other side: propagating a forward-going track to smaller z simply runs the equations backwards.
   *
   * See docs/kalman_filter.md, section "Propagation".
   */
  std::optional<propagation_result> propagate(const state& in, double z_target, double mass, const field& bfield,
                                              const propagator_config& cfg);

} // namespace sand::tracker::kalman
