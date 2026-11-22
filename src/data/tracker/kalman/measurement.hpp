#pragma once

#include <kalman/state.hpp>

#include <cmath>

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
 *      t_TDC = t + L / v_wire + |d| / v_drift + t_offset
 *    with t the track time on the wire plane, d the signed distance of closest approach between track and wire and L
 *    the distance from the closest approach point to the readout end. Used in the final fit, with the left-right side
 *    given explicitly (+1 or -1 for the sign of d).
 *
 * Both return the predicted value and the 1 x 6 derivative row H with respect to the state on the wire plane. The
 * state passed in must already be on the wire plane (state.z == wire z).
 */

namespace sand::tracker::kalman {

  /**
   * @brief Geometry of one wire, as used by the measurement models.
   *
   * Filled by the reconstruction modules from the wire description of the geometry service; the library does not
   * depend on it.
   */
  struct wire_geometry {
    double head[3]; ///< Readout end [mm]
    double tail[3]; ///< Far end [mm]

    /// Plane of the wire [mm]
    double z() const { return 0.5 * (head[2] + tail[2]); }
    /// Length of the wire [mm]
    double length() const {
      double const dx = tail[0] - head[0], dy = tail[1] - head[1];
      return std::sqrt(dx * dx + dy * dy);
    }
    /// Unit vector along the wire, from head to tail (in the xy plane)
    void along(double& ux, double& uy) const {
      double const l = length();
      ux             = (tail[0] - head[0]) / l;
      uy             = (tail[1] - head[1]) / l;
    }
  };

  /// What the reconstruction assumes about the detector response. Distinct from the simulation parameters.
  struct reco_calibration {
    double drift_velocity = 0.05; ///< [mm/ns]
    double wire_velocity  = 200.; ///< Signal propagation along the wire [mm/ns]
    double tdc_resolution = 3.5;  ///< [ns]
    double t0_offset      = 0.;   ///< Fixed electronics offset [ns]

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
   *
   * @param s State on the wire plane
   * @param w Wire
   */
  prediction wire_position(const state& s, const wire_geometry& w);

  namespace detail {

    /**
     * @brief Signed distance of closest approach d and distance L from the readout end, for a straight local track.
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
   * The derivatives are evaluated with the left-right side fixed: |d| is replaced by side * d, so that the model is
   * smooth across the wire. This is what the deterministic annealing filter needs, which treats the two sides as two
   * competing measurements.
   *
   * @param s    State on the wire plane
   * @param w    Wire
   * @param side Left-right hypothesis: +1 or -1, the sign of the distance d
   * @param cal  Reconstruction calibration
   */
  prediction wire_time(const state& s, const wire_geometry& w, int side, const reco_calibration& cal);

} // namespace sand::tracker::kalman
