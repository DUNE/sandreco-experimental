#include <kalman/measurement.hpp>

#include <cmath>

namespace sand::tracker::kalman {

  prediction wire_position(const state& s, const wire_geometry& w) {
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

    void wire_distance(const vector_t& par, const wire_geometry& w, double& d, double& L) {
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

  prediction wire_time(const state& s, const wire_geometry& w, int side, const reco_calibration& cal) {
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
