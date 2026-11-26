#pragma once

#include <kalman/field.hpp>
#include <kalman/state.hpp>

#include <optional>

/**
 * @file propagator.hpp
 * @brief Runge-Kutta propagation of the track state in z, with the transport Jacobian.
 *
 * Equations of motion with z as independent variable, sigma = sign(p_z), v = (tx, ty, 1), |v| = sqrt(1 + tx^2 + ty^2),
 * kappa = CLHEP::c_light * CLHEP::tesla (field B in T, see state.hpp), c = CLHEP::c_light:
 *
 *   dx/dz  = tx
 *   dy/dz  = ty
 *   dtx/dz = sigma * kappa * (q/p) * |v| * ( tx*ty*Bx - (1 + tx^2)*By + ty*Bz )
 *   dty/dz = sigma * kappa * (q/p) * |v| * ( (1 + ty^2)*Bx - tx*ty*By - tx*Bz )
 *   dqop/dz = 0                                       (no energy loss: the library has no material model yet)
 *   dt/dz  = sigma * |v| * sqrt(1 + m^2 (q/p)^2) / c   (time of flight for the mass hypothesis m)
 *
 * These six derivatives form the right-hand side phi (f in the code) of dx/dz = phi(x, z), with x the state vector.
 * For a straight track (q/p = 0) the time of flight uses beta = 1 and the mass hypothesis has no effect.
 *
 * The equations are integrated with the classical fourth-order Runge-Kutta scheme; the field is read at the position
 * of each of the four stages, so the scheme is a true RK4 in a non-uniform field too. The Jacobian
 * F = d(state at z1)/d(state at z0) is integrated together with the state with the same stages, using dF/dz = A F
 * with A = dphi/dx. The derivatives of the field with respect to the position are neglected in A, which is exact for
 * a uniform field.
 *
 * The derivation of these equations and of the Jacobian is in docs/kalman_filter.md, sections "2.2 Propagator f" and
 * "2.3 Propagation matrix F".
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
    double path_length{0}; ///< Length travelled [mm]; not used by the library yet (intended for a material model)
  };

  namespace detail {

    /**
     * @brief Right-hand side phi (f in the code) and its Jacobian A = dphi/dx for the equations above.
     *
     * See docs/kalman_filter.md, sections "2.2 Propagator f" and "2.3 Propagation matrix F".
     * @param s     State vector
     * @param sigma Sign of p_z
     * @param mass  Mass hypothesis [MeV]
     * @param b     Field at the position of s [T]
     * @param f     Output: phi = d(state)/dz
     * @param A     Output: A = dphi/d(state)
     */
    void derivatives(const vector_t& s, int sigma, double mass, const field_vector& b, vector_t& f, matrix_t& A);

  } // namespace detail

  /**
   * @brief Propagate a state to the plane z_target.
   * @param in      Input state (its covariance is transported).
   * @param z_target Target plane [mm]. May be upstream or downstream of in.z.
   * @param mass    Mass hypothesis [MeV], used only for the time of flight (no effect for q/p = 0, where beta = 1).
   * @param bfield  Magnetic field.
   * @param cfg     Runge-Kutta settings (no default: from the configuration).
   * @return The propagated state and Jacobian, or nothing if the track becomes too steep.
   * @throws std::invalid_argument if cfg.max_step or cfg.max_slope is not positive (for example a value-initialised
   *         propagator_config).
   *
   * The direction of motion is a property of the particle (in.direction) and is not changed by propagating to a
   * plane on the other side: propagating a forward-going track to smaller z simply runs the equations backwards.
   *
   * See docs/kalman_filter.md, section "2.2 Propagator f".
   */
  std::optional<propagation_result> propagate(const state& in, double z_target, double mass, const field& bfield,
                                              const propagator_config& cfg);

} // namespace sand::tracker::kalman
