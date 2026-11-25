#pragma once

#include <kalman/state.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <array>

/**
 * @file measurement.hpp
 * @brief Measurement models for a single tracker wire.
 *
 * The same model describes a drift-chamber wire and a straw tube: it is the model of the fast digitizations
 * drift_fast_digi and stt_fast_digi (drift time to the closest approach plus signal propagation along the wire).
 *
 * A wire is a straight segment in a plane of constant z, from the readout end (head) to the far end (tail). The
 * model only uses the wire geometry, so it works for any number of views and any stereo angle.
 *
 * Two models are provided:
 *  - wire_position: the wire is a coordinate measurement across the wire, with a flat error. Used in track finding.
 *  - wire_time: the measurement is the TDC time, predicted as
 *      t_TDC = t + L / v_wire + side * d / v_drift + t_offset
 *    with t the track time on the wire plane, d the signed distance of closest approach between track and wire, L
 *    the distance from the closest approach point to the readout end and side the left-right hypothesis. Used in the
 *    final fit.
 *
 * Both return the predicted value and the 1 x 6 derivative row H with respect to the state on the wire plane. The
 * state passed in must already be on the wire plane (state.z == wire z).
 *
 * See docs/kalman_filter.md, section "Wire measurements", for the formulas.
 */

namespace sand::tracker::kalman {

  /**
   * @brief Largest difference in z allowed between the two ends of a wire.
   *
   * A geometric sanity check of the wire description, not a physics or configuration value: the measurement models
   * assume that the wire lies in a plane of constant z, and the geometry describes wires that do, up to rounding.
   */
  inline constexpr double s_wire_plane_tolerance = 1. * CLHEP::micrometer;

  /**
   * @brief Geometry of one wire, as used by the measurement models.
   *
   * Filled by the reconstruction modules from the wire description of the geometry service; the library does not
   * depend on it. The constructor checks that the wire lies in a plane of constant z and has a non-zero length, so a
   * wire_geometry always describes a valid wire.
   */
  class wire_geometry {
   public:
    using point = std::array<double, 3>; ///< Global position (x, y, z) [mm]

    /**
     * @brief Wire from its two ends.
     * @param head Readout end [mm]
     * @param tail Far end [mm]
     * @throws std::invalid_argument if |head.z - tail.z| > s_wire_plane_tolerance, or if the wire has zero length
     */
    wire_geometry(const point& head, const point& tail);

    /// Readout end [mm]
    const point& head() const { return m_head; }
    /// Far end [mm]
    const point& tail() const { return m_tail; }
    /// Plane of the wire [mm]
    double z() const { return 0.5 * (m_head[2] + m_tail[2]); }
    /// Length of the wire [mm]
    double length() const { return m_length; }
    /// Unit vector along the wire, from head to tail (in the xy plane)
    void along(double& ux, double& uy) const {
      ux = m_ux;
      uy = m_uy;
    }

   private:
    point m_head;
    point m_tail;
    double m_length;
    double m_ux;
    double m_uy;
  };

  /**
   * @brief What the reconstruction assumes about the detector response. Distinct from the simulation parameters.
   *
   * No default values: every tunable parameter comes from the configuration (keys `calibration.*` of the engine).
   * Code that builds the struct directly sets every member.
   */
  struct reco_calibration {
    double drift_velocity; ///< [mm/ns]
    double wire_velocity;  ///< Signal propagation along the wire [mm/ns]
    double tdc_resolution; ///< [ns]
    double t0_offset;      ///< Fixed electronics offset [ns]

    /// Drift time [ns] for a distance r [mm] from the wire
    double drift_time(double r) const { return r / drift_velocity; }
    /// Drift distance [mm] for a drift time t [ns]
    double drift_distance(double t) const { return t * drift_velocity; }
  };

  /// Predicted measurement and its derivative with respect to the state.
  struct prediction {
    double value{0}; ///< Predicted measurement
    row_t H{};       ///< d(value)/d(state)
  };

  /**
   * @brief Signed coordinate of the track across the wire, on the wire plane.
   *
   * The coordinate is n . (x - head) with n = (-uy, ux) the in-plane normal to the wire. The measured value for a
   * wire with no drift information is 0, with variance pitch^2 / 12 or max_radius^2 / 3.
   * See docs/kalman_filter.md, section "Wire measurements".
   *
   * @param s State on the wire plane
   * @param w Wire
   */
  prediction wire_position(const state& s, const wire_geometry& w);

  namespace detail {

    /**
     * @brief Signed distance of closest approach d and distance L from the readout end, for a straight local track.
     *
     * Near the wire plane the track is the straight line P(lambda) = P0 + lambda v, with v = (tx, ty, 1) and
     * P0 = (x, y, z) its point on the wire plane. The wire is W(mu) = H + mu u, with H the readout end and
     * u = (ux, uy, 0) the unit vector from head to tail. Delta = P0 - H has zero z component (same plane).
     *
     * The signed distance of closest approach is the projection of Delta on the common normal v x u:
     *
     *     d = Delta . (v x u) / |v x u|
     *
     * with v x u = (-uy, ux, uy tx - ux ty) and |v x u| = sqrt(1 + t_n^2), which gives
     *
     *     d = n . Delta / sqrt(1 + t_n^2),   n = (-uy, ux),   t_n = n . (tx, ty)
     *
     * d > 0 when the track passes on the side of n. L is the value of mu that minimises
     * |P0 + lambda v - H - mu u|^2 over lambda and mu:
     *
     *     L = (Delta.u - (Delta.v)(u.v)/(v.v)) / (1 - (u.v)^2/(v.v))
     *
     * See docs/kalman_filter.md, section "Wire measurements".
     *
     * @param par State vector on the wire plane
     * @param w   Wire
     * @param d   Output: signed distance of closest approach [mm]
     * @param L   Output: distance of the closest point on the wire from the readout end [mm]
     */
    void wire_distance(const vector_t& par, const wire_geometry& w, double& d, double& L);

  } // namespace detail

  /**
   * @brief Predicted TDC time for a wire, and its derivatives.
   *
   * The model is
   *
   *     t_TDC = t + L / v_wire + side * d / v_drift + t_offset
   *
   * with d and L from detail::wire_distance(). The drift time is |d| / v_drift, but |d| is not differentiable at
   * d = 0, so the model uses side * d with a fixed left-right hypothesis. With the right hypothesis (side = sign of d)
   * the term equals |d| / v_drift and is never subtracted. With the wrong one it predicts a negative drift time: the
   * residual is large and the deterministic annealing filter, which treats the two sides as competing measurements,
   * gives that hypothesis a low weight.
   *
   * The derivative row H is computed analytically from the same side-fixed model. See docs/kalman_filter.md, section
   * "Wire measurements".
   *
   * @param s    State on the wire plane
   * @param w    Wire
   * @param side Left-right hypothesis: +1 or -1, the sign of the distance d
   * @param cal  Reconstruction calibration
   */
  prediction wire_time(const state& s, const wire_geometry& w, int side, const reco_calibration& cal);

} // namespace sand::tracker::kalman
