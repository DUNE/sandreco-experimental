#pragma once

#include <tracker/kalman/state.hpp>

#include <cmath>

/**
 * @file measurement.hpp
 * @brief Measurement models for a single drift-chamber wire.
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
   */
  inline prediction wire_position(const state& s, const wire_geometry& w) {
    double ux, uy;
    w.along(ux, uy);
    double const nx = -uy, ny = ux;
    prediction p;
    p.value = nx * (s.par[X] - w.head[0]) + ny * (s.par[Y] - w.head[1]);
    p.H[X]  = nx;
    p.H[Y]  = ny;
    return p;
  }

  namespace detail {

    /// Signed distance of closest approach d and distance L from the readout end, for a straight local track.
    inline void wire_distance(const vector_t& par, const wire_geometry& w, double& d, double& L) {
      double ux, uy;
      w.along(ux, uy);
      double const nx = -uy, ny = ux;
      double const dx = par[X] - w.head[0], dy = par[Y] - w.head[1];
      double const tn = nx * par[TX] + ny * par[TY]; // track slope across the wire
      d               = (nx * dx + ny * dy) / std::sqrt(1. + tn * tn);

      // Closest point on the wire line: minimise |P + l v - (H + s u)| over l and s, with v = (tx, ty, 1)
      double const vv = 1. + par[TX] * par[TX] + par[TY] * par[TY];
      double const uv = ux * par[TX] + uy * par[TY];
      double const du = dx * ux + dy * uy;
      double const dv = dx * par[TX] + dy * par[TY];
      L               = (du - dv * uv / vv) / (1. - uv * uv / vv);
    }

  } // namespace detail

  /**
   * @brief Predicted TDC time for a wire, and its derivatives.
   *
   * The derivatives are evaluated with the left-right side fixed: |d| is replaced by side * d, so that the model is
   * smooth across the wire. This is what the deterministic annealing filter needs, which treats the two sides as two
   * competing measurements.
   */
  /**
   * @param s    State on the wire plane
   * @param w    Wire
   * @param side Left-right hypothesis: +1 or -1, the sign of the distance d
   * @param cal  Reconstruction calibration
   */
  inline prediction wire_time(const state& s, const wire_geometry& w, int side, const reco_calibration& cal) {
    double d, L;
    detail::wire_distance(s.par, w, d, L);
    prediction p;
    p.value = s.par[T] + L / cal.wire_velocity + side * d / cal.drift_velocity + cal.t0_offset;

    // Derivatives by central differences of the smooth (side-fixed) model; the model is cheap and exact formulas
    // would add little. Steps are small compared with the scales of each parameter.
    static constexpr double eps[s_dim] = {1e-3, 1e-3, 1e-6, 1e-6, 1e-9, 1e-3};
    for (unsigned int i = 0; i < s_dim; ++i) {
      vector_t up = s.par, dn = s.par;
      up[i] += eps[i];
      dn[i] -= eps[i];
      double du, lu, dd, ld;
      detail::wire_distance(up, w, du, lu);
      detail::wire_distance(dn, w, dd, ld);
      double const vu = up[T] + lu / cal.wire_velocity + side * du / cal.drift_velocity;
      double const vd = dn[T] + ld / cal.wire_velocity + side * dd / cal.drift_velocity;
      p.H[i]          = (vu - vd) / (2. * eps[i]);
    }
    return p;
  }

} // namespace sand::tracker::kalman
