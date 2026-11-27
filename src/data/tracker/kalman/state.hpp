#pragma once

#include <kalman/particle.hpp>

#include <Geant4/CLHEP/Units/PhysicalConstants.h>
#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <Math/SMatrix.h>
#include <Math/SVector.h>

#include <cmath>

/**
 * @file state.hpp
 * @brief Track state on a plane at fixed z, used by the tracker Kalman filter.
 *
 * Units used throughout sand::tracker::kalman:
 *  - lengths in mm, times in ns, momenta and masses in MeV: the CLHEP internal units, so values from CLHEP (for
 *    example CLHEP::c_light in mm/ns) can be used directly;
 *  - magnetic field in T, as a plain number: in the field interface, in the engine configuration and in the values of
 *    the GDML. It is not converted to CLHEP internal units (where 1 T = CLHEP::tesla = 0.001).
 *
 * Physical constants come from CLHEP (Geant4/CLHEP/Units/PhysicalConstants.h and SystemOfUnits.h), with qualified
 * names; masses and charges come from the particle table through hypothesis_from_pdg() (particle.hpp). The curvature
 * constant is not
 * written as a number: it is derived as
 *
 *   kappa = CLHEP::c_light * CLHEP::tesla = 0.299792458 MeV / (mm T)
 *
 * so that p [MeV] = kappa * |q| * B [T] * R [mm], q in units of e. See docs/kalman_filter.md, section
 * "0.3 Field and curvature constant kappa".
 *
 * The state vector is (x, y, tx, ty, q/p, t) at the reference plane z:
 *  - x, y   : position on the plane [mm]
 *  - tx, ty : slopes dx/dz, dy/dz
 *  - q/p    : charge over momentum [1/MeV], charge in units of e. The curvature depends on q/p only; the momentum
 *             p = |q| / |q/p| needs the absolute charge of the particle hypothesis (momentum(), particle.hpp)
 *  - t      : time at which the particle crosses the plane [ns]
 *
 * The time parameter is the track time on the reference plane: the track t0, in the sense used by the
 * reconstruction, is the value of t on the first (or vertex) plane.
 *
 * Since z is the independent variable, the state also carries the sign of p_z (direction, sigma in the formulas): +1
 * for a particle moving towards larger z, -1 for one moving towards smaller z.
 *
 * Since z is the independent variable, a track is described on planes of fixed z only: a track parallel to the planes
 * (p_z = 0) cannot be represented, and the propagation stops when |tx| or |ty| exceed propagator_config::max_slope.
 *
 * See docs/kalman_filter.md, section "2.1 State x and covariance C".
 */

/**
 * @namespace sand::tracker::kalman
 * @brief Kalman filter library for the SAND tracker: track state, propagation in a magnetic field, wire measurement
 * models, update and smoother. The measurement model applies to drift chambers and straw tubes alike. Independent of
 * the digit format and of the geometry service; the shared state (field, calibration, propagation settings) is held
 * by the complex data object sand::tracker::kalman::engine.
 */
/**
 * @namespace sand::tracker::kalman::detail
 * @brief Helpers that are not part of the public interface of the library.
 *
 * The functions in this namespace (detail::derivatives(), detail::wire_distance()) are building blocks of the public
 * functions propagate() and wire_time(). Reconstruction modules should not call them. They are declared in the
 * headers, and not hidden in the .cpp files, only because the standalone tests check them directly against analytic
 * results and finite differences. See docs/kalman_filter.md, section "3.5 Public interface and namespace detail".
 */

namespace sand::tracker::kalman {

  inline constexpr unsigned int s_dim = 6; ///< Dimension of the state vector

  /// Indices of the state vector components.
  enum par : unsigned int { X = 0, Y = 1, TX = 2, TY = 3, QOP = 4, T = 5 };

  using vector_t     = ROOT::Math::SVector<double, s_dim>;        ///< State vector
  using matrix_t     = ROOT::Math::SMatrix<double, s_dim, s_dim>; ///< General 6 x 6 matrix (Jacobians)
  using sym_matrix_t = ROOT::Math::SMatrix<double, s_dim, s_dim, ROOT::Math::MatRepSym<double, s_dim>>; ///< Covariance
  using row_t        = ROOT::Math::SVector<double, s_dim>; ///< A 1 x s_dim measurement matrix, stored as a vector

  /**
   * @brief Track state on the plane z: parameters, covariance and direction of motion.
   *
   * See docs/kalman_filter.md, section "2.1 State x and covariance C", for the parameters, their units and the
   * covariance.
   */
  struct state {
    double z{0};        ///< Reference plane [mm]
    vector_t par{};     ///< (x, y, tx, ty, q/p, t)
    sym_matrix_t cov{}; ///< Covariance of par
    int direction{+1};  ///< Sign of p_z

    /// Sign of q/p, that is the sign of the charge: +1, -1, or 0 for a straight track
    int charge() const { return par[QOP] > 0 ? +1 : (par[QOP] < 0 ? -1 : 0); }
    /// Unit vector of the direction of motion, including the sign of p_z
    void direction_cosines(double& ux, double& uy, double& uz) const {
      double const n = std::sqrt(1. + par[TX] * par[TX] + par[TY] * par[TY]);
      ux             = direction * par[TX] / n;
      uy             = direction * par[TY] / n;
      uz             = direction / n;
    }
  };

  /**
   * @brief Momentum of the state for a particle hypothesis: p = |q| / |q/p|.
   *
   * A free function of the state and of the hypothesis, rather than a member of state, because the state carries
   * only q/p: the momentum needs the absolute charge, which is part of the hypothesis (an alpha has |q| = 2). The mass
   * of the hypothesis is not used.
   *
   * See docs/kalman_filter.md, section "2.1 State x and covariance C".
   * @param s   State
   * @param hyp Particle hypothesis; only hyp.abs_charge is used
   * @return Momentum [MeV]; HUGE_VAL for q/p = 0 (straight track)
   */
  inline double momentum(const state& s, const particle_hypothesis& hyp) {
    return s.par[QOP] != 0 ? hyp.abs_charge / std::abs(s.par[QOP]) : HUGE_VAL;
  }

} // namespace sand::tracker::kalman
