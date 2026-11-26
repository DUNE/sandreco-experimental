#include <kalman/measurement.hpp>

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace sand::tracker::kalman {

  wire_geometry::wire_geometry(const point& head, const point& tail) : m_head(head), m_tail(tail) {
    double const dz = tail[2] - head[2];
    // Written as !(... <= ...) so that a NaN coordinate is rejected too
    if (!(std::abs(dz) <= s_wire_plane_tolerance)) {
      std::ostringstream msg;
      msg << "sand::tracker::kalman::wire_geometry: the wire ends are not at the same z (head z = " << head[2]
          << " mm, tail z = " << tail[2] << " mm, tolerance " << s_wire_plane_tolerance << " mm)";
      throw std::invalid_argument(msg.str());
    }
    double const dx = tail[0] - head[0], dy = tail[1] - head[1];
    m_length = std::sqrt(dx * dx + dy * dy);
    if (!(m_length > 0.)) {
      std::ostringstream msg;
      msg << "sand::tracker::kalman::wire_geometry: zero-length wire at (" << head[0] << ", " << head[1] << ", "
          << head[2] << ") mm";
      throw std::invalid_argument(msg.str());
    }
    m_ux = dx / m_length;
    m_uy = dy / m_length;
  }

  prediction wire_position(const state& s, const wire_geometry& w) {
    double ux, uy;
    w.along(ux, uy);
    double const nx = -uy, ny = ux;
    prediction p;
    p.value = nx * (s.par[X] - w.head()[0]) + ny * (s.par[Y] - w.head()[1]);
    p.H[X]  = nx;
    p.H[Y]  = ny;
    return p;
  }

  namespace detail {

    void wire_distance(const vector_t& par, const wire_geometry& w, double& d, double& L) {
      // Notation of the header and of docs/kalman_filter.md: Delta = P0 - W0, v = (tx, ty, 1), u = (ux, uy, 0)
      double ux, uy;
      w.along(ux, uy);
      double const nx = -uy, ny = ux;
      double const dx = par[X] - w.head()[0], dy = par[Y] - w.head()[1];
      double const tn = nx * par[TX] + ny * par[TY]; // t_n = n . (tx, ty), slope across the wire
      d               = (nx * dx + ny * dy) / std::sqrt(1. + tn * tn);

      double const vv = 1. + par[TX] * par[TX] + par[TY] * par[TY]; // v . v
      double const uv = ux * par[TX] + uy * par[TY];                // u . v
      double const du = dx * ux + dy * uy;                          // Delta . u
      double const dv = dx * par[TX] + dy * par[TY];                // Delta . v
      L               = (du - dv * uv / vv) / (1. - uv * uv / vv);
    }

  } // namespace detail

  prediction wire_time(const state& s, const wire_geometry& w, int side, const reco_calibration& cal) {
    double d, L;
    detail::wire_distance(s.par, w, d, L);
    prediction p;
    p.value = s.par[T] + L / cal.wire_velocity + side * d / cal.drift_velocity + cal.t0_offset;

    // Analytic derivatives of the side-fixed model. In the frame of the wire, with a = Delta . u, b = Delta . n,
    // t_u = u . (tx, ty), t_n = n . (tx, ty) and rho^2 = 1 + t_n^2 (see docs/kalman_filter.md, "Wire measurements"):
    //   d = b / rho,   L = a - b t_n t_u / rho^2
    // (the second is the formula of wire_distance, using v . v = 1 + t_u^2 + t_n^2). x and y enter through a and b,
    // tx and ty through t_u and t_n; q/p does not enter.
    double ux, uy;
    w.along(ux, uy);
    double const nx = -uy, ny = ux;
    double const b    = nx * (s.par[X] - w.head()[0]) + ny * (s.par[Y] - w.head()[1]);
    double const tu   = ux * s.par[TX] + uy * s.par[TY];
    double const tn   = nx * s.par[TX] + ny * s.par[TY];
    double const rho2 = 1. + tn * tn;
    double const rho  = std::sqrt(rho2);

    double const dd_db  = 1. / rho;
    double const dd_dtn = -b * tn / (rho2 * rho);
    double const dL_da  = 1.;
    double const dL_db  = -tn * tu / rho2;
    double const dL_dtu = -b * tn / rho2;
    double const dL_dtn = -b * tu * (1. - tn * tn) / (rho2 * rho2);

    // d(t_TDC) with respect to a, b, t_u, t_n
    double const sv  = side / cal.drift_velocity;
    double const iw  = 1. / cal.wire_velocity;
    double const dta = iw * dL_da;
    double const dtb = iw * dL_db + sv * dd_db;
    double const dtu = iw * dL_dtu;
    double const dtn = iw * dL_dtn + sv * dd_dtn;

    p.H[X]   = ux * dta + nx * dtb;
    p.H[Y]   = uy * dta + ny * dtb;
    p.H[TX]  = ux * dtu + nx * dtn;
    p.H[TY]  = uy * dtu + ny * dtn;
    p.H[QOP] = 0.;
    p.H[T]   = 1.;
    return p;
  }

} // namespace sand::tracker::kalman
