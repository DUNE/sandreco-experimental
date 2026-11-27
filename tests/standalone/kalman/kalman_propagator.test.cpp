#define BOOST_TEST_MODULE kalman_propagator
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>
#include <kalman/particle.hpp>
#include <kalman/propagator.hpp>

#include <Geant4/CLHEP/Units/PhysicalConstants.h>
#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

using namespace sand::tracker::kalman;

namespace {
  particle_hypothesis const mu = hypothesis_from_pdg(13); // muon: mass from the table, |q| = 1

  /// p [MeV] = kappa * B [T] * R [mm], as in the propagator
  double constexpr kappa = CLHEP::c_light * CLHEP::tesla;

  /// Runge-Kutta settings of the tests that do not study them
  propagator_config const cfg{10., 1e3};

  state make_state(double x, double y, double tx, double ty, double qop, double t, int dir = +1) {
    state s;
    s.z         = 0.;
    s.par[X]    = x;
    s.par[Y]    = y;
    s.par[TX]   = tx;
    s.par[TY]   = ty;
    s.par[QOP]  = qop;
    s.par[T]    = t;
    s.direction = dir;
    return s;
  }

  /// PDG code of the alpha particle (4He nucleus)
  int constexpr alpha_pdg = 1000020040;

  /// Hypothesis of the alpha. The default particle table of ROOT has no nuclei: the test adds the alpha to it, as a job
  /// that fits nuclei would, so that hypothesis_from_pdg() and its conversion of the charge from units of |e|/3 are
  /// exercised. Mass: CODATA 2022 alpha particle mass energy equivalent, 3727.3794118 MeV (in GeV for the table).
  particle_hypothesis alpha() {
    auto* db = TDatabasePDG::Instance();
    if (db->GetParticle(alpha_pdg) == nullptr) {
      db->AddParticle("alpha", "alpha", 3.7273794118, true, 0., 6., "Ion", alpha_pdg);
    }
    return hypothesis_from_pdg(alpha_pdg);
  }
} // namespace

BOOST_AUTO_TEST_SUITE(propagator)

// A track starting along z in a field along x stays in the yz plane and follows a circle of radius
// R = p_{perp} / (kappa |q| B), with p_{perp} the momentum component perpendicular to the field: here the track is
// perpendicular to the field, so p_{perp} = p, and |q| = 1. For a negative charge the force q v x B points towards -y.
BOOST_AUTO_TEST_CASE(helix_in_uniform_field) {
  double const p = 1000., B = 0.6, z1 = 2000.;
  uniform_field const bf({B, 0., 0.});
  for (int q : {-1, +1}) {
    auto const in  = make_state(0., 0., 0., 0., q / p, 0.);
    auto const res = propagate(in, z1, mu, bf, {10., 1e3});
    BOOST_REQUIRE(res);
    double const R    = p / (kappa * std::abs(q) * B); // p_{perp} = p
    double const root = std::sqrt(R * R - z1 * z1);
    double const y    = q * (R - root);
    double const ty   = q * z1 / root;
    double const s    = R * std::asin(z1 / R);
    double const beta = p / std::sqrt(p * p + mu.mass * mu.mass);
    BOOST_CHECK_SMALL(res->out.par[X], 1e-9);
    BOOST_CHECK_CLOSE(res->out.par[Y], y, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[TY], ty, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[T], s / (beta * CLHEP::c_light), 1e-6);
    BOOST_CHECK_CLOSE(res->path_length, s, 1e-4);
  }
}

// An alpha (|q| = 2) along z in a field along x follows a circle of radius R = p / (kappa |q| B), with q/p = 2/p in the
// state: the curvature depends on q/p only. Its time of flight needs 1/beta = E/p = sqrt(1 + m^2/p^2), that is
// sqrt(1 + (m/|q|)^2 (q/p)^2), and its momentum is |q| / |q/p|. Both are compared with the exact kinematics.
BOOST_AUTO_TEST_CASE(alpha_time_of_flight_and_momentum) {
  auto const he = alpha();
  BOOST_CHECK_EQUAL(he.abs_charge, 2.);
  BOOST_CHECK_CLOSE(he.mass, 3727.3794118, 1e-12);
  double const p = 1000., B = 0.6, z1 = 2000., q = +2.;
  uniform_field const bf({B, 0., 0.});
  auto const in = make_state(0., 0., 0., 0., q / p, 0.);
  BOOST_CHECK_CLOSE(momentum(in, he), p, 1e-12);
  BOOST_CHECK_CLOSE(momentum(in, mu), p / 2., 1e-12); // the same q/p read as a unit charge
  BOOST_CHECK_EQUAL(momentum(make_state(0., 0., 0., 0., 0., 0.), he), HUGE_VAL);
  auto const res = propagate(in, z1, he, bf, {10., 1e3});
  BOOST_REQUIRE(res);
  double const R    = p / (kappa * q * B);
  double const root = std::sqrt(R * R - z1 * z1);
  double const s    = R * std::asin(z1 / R);
  double const beta = p / std::sqrt(p * p + he.mass * he.mass);
  BOOST_CHECK_CLOSE(res->out.par[Y], R - root, 1e-6);
  BOOST_CHECK_CLOSE(res->out.par[TY], z1 / root, 1e-6);
  BOOST_CHECK_CLOSE(res->out.par[T], s / (beta * CLHEP::c_light), 1e-6);
  BOOST_CHECK_CLOSE(momentum(res->out, he), p, 1e-12);
  // The unit-charge formula sqrt(1 + m^2 (q/p)^2) would give a time about twice as long for this alpha
  double const beta_unit = 1. / std::sqrt(1. + he.mass * he.mass * (q / p) * (q / p));
  BOOST_CHECK_GT(s / (beta_unit * CLHEP::c_light) / res->out.par[T], 1.9);
  // The mass and |q| enter only as m/|q|: an alpha and a unit charge of mass m/2 have the same time of flight
  auto const half = propagate(in, z1, particle_hypothesis{he.mass / 2., 1.}, bf, {10., 1e3});
  BOOST_REQUIRE(half);
  BOOST_CHECK_CLOSE(half->out.par[T], res->out.par[T], 1e-12);
  BOOST_CHECK_CLOSE(half->out.par[Y], res->out.par[Y], 1e-12);
}

// The analytic A = dphi/dx of detail::derivatives(), element by element, against central differences of phi, for a
// general field (three components), both directions, a unit charge and an alpha. The columns of x, y and t and the row
// of q/p are exactly zero: phi does not depend on x, y, t once the field gradients are neglected, and q/p is constant.
BOOST_AUTO_TEST_CASE(derivatives_vs_finite_differences) {
  std::mt19937 rng(4321);
  std::uniform_real_distribution<double> u(-1., 1.);
  particle_hypothesis const hyps[] = {mu, alpha(), hypothesis_from_pdg(2212)};
  for (int n = 0; n < 200; ++n) {
    vector_t s;
    s[X]   = 1000. * u(rng);
    s[Y]   = 1000. * u(rng);
    s[TX]  = u(rng);
    s[TY]  = u(rng);
    s[QOP] = u(rng) / 200.; // |p| down to 200 MeV for |q| = 1
    s[T]   = 10. * u(rng);
    field_vector const b{0.6 + 0.2 * u(rng), 0.2 * u(rng), 0.2 * u(rng)};
    int const sigma = n % 2 ? +1 : -1;
    auto const& hyp = hyps[n % 3];
    vector_t f;
    matrix_t A;
    detail::derivatives(s, sigma, hyp, b, f, A);
    double const eps[s_dim] = {1e-2, 1e-2, 1e-6, 1e-6, 1e-9, 1e-3};
    for (unsigned int j = 0; j < s_dim; ++j) {
      vector_t up = s, dn = s, fu, fd;
      matrix_t Au, Ad;
      up[j] += eps[j];
      dn[j] -= eps[j];
      detail::derivatives(up, sigma, hyp, b, fu, Au);
      detail::derivatives(dn, sigma, hyp, b, fd, Ad);
      for (unsigned int i = 0; i < s_dim; ++i) {
        double const num = (fu[i] - fd[i]) / (2. * eps[j]);
        if (j == X || j == Y || j == T || i == QOP) {
          BOOST_CHECK_EQUAL(A(i, j), 0.);
          BOOST_CHECK_EQUAL(num, 0.);
          continue;
        }
        // Truncation error of order eps^2, rounding of order 1e-16 |f| / eps
        double const tol = 1e-6 * std::abs(A(i, j)) + 1e-15 * std::max(1., std::abs(f[i])) / eps[j];
        BOOST_CHECK_MESSAGE(std::abs(A(i, j) - num) <= tol, "A(" << i << "," << j << ") = " << A(i, j) << " vs " << num
                                                                 << " (m " << hyp.mass << ", |q| " << hyp.abs_charge
                                                                 << ", sigma " << sigma << ")");
      }
    }
  }
}

// Propagating forward and back must return to the start, and the two Jacobians must be inverse to each other.
BOOST_AUTO_TEST_CASE(round_trip) {
  uniform_field const bf({0.6, 0.05, -0.02});
  auto const in = make_state(10., -20., 0.3, -0.2, -1. / 700., 5.);
  auto const fw = propagate(in, 1500., mu, bf, {20., 1e3});
  BOOST_REQUIRE(fw);
  auto const bw = propagate(fw->out, 0., mu, bf, {20., 1e3});
  BOOST_REQUIRE(bw);
  for (unsigned int i = 0; i < s_dim; ++i) {
    BOOST_CHECK_SMALL(bw->out.par[i] - in.par[i], 1e-7);
  }
  // The Jacobian carries the Runge-Kutta truncation error, of order 1e-5 with 20 mm steps over 1.5 m
  matrix_t const prod = bw->F * fw->F;
  for (unsigned int i = 0; i < s_dim; ++i) {
    for (unsigned int j = 0; j < s_dim; ++j) {
      BOOST_CHECK_SMALL(prod(i, j) - (i == j ? 1. : 0.), 1e-4);
    }
  }
}

// The integrated Jacobian must agree with finite differences of the propagation.
BOOST_AUTO_TEST_CASE(jacobian_vs_finite_differences) {
  uniform_field const bf({0.6, 0.05, -0.02});
  for (int dir : {+1, -1}) {
    auto const in  = make_state(10., -20., 0.3, -0.2, -1. / 400., 5., dir);
    double const z = dir > 0 ? 1200. : -1200.;
    auto const ref = propagate(in, z, mu, bf, {10., 1e3});
    BOOST_REQUIRE(ref);
    double const eps[s_dim] = {1e-3, 1e-3, 1e-6, 1e-6, 1e-9, 1e-3};
    for (unsigned int j = 0; j < s_dim; ++j) {
      auto up = in, dn = in;
      up.par[j] += eps[j];
      dn.par[j] -= eps[j];
      auto const ru = propagate(up, z, mu, bf, {10., 1e3});
      auto const rd = propagate(dn, z, mu, bf, {10., 1e3});
      BOOST_REQUIRE(ru && rd);
      for (unsigned int i = 0; i < s_dim; ++i) {
        double const fd    = (ru->out.par[i] - rd->out.par[i]) / (2. * eps[j]);
        double const scale = std::max(1., std::abs(fd));
        BOOST_CHECK_MESSAGE(std::abs(ref->F(i, j) - fd) < 1e-4 * scale,
                            "dir " << dir << " F(" << i << "," << j << ") = " << ref->F(i, j) << " vs " << fd);
      }
    }
  }
}

// A negative particle moving along +z in a field along x is pushed towards -y; moving along -z it is pushed towards
// +y. After the same path length the two are mirror images: y changes sign, dy/dz does not, and both are later in time.
BOOST_AUTO_TEST_CASE(backward_moving_particle) {
  double const p = 1000., B = 0.6;
  uniform_field const bf({B, 0., 0.});
  auto const fwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., +1), 1000., mu, bf, cfg);
  auto const bwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., -1), -1000., mu, bf, cfg);
  BOOST_REQUIRE(fwd && bwd);
  BOOST_CHECK_LT(fwd->out.par[Y], 0.);
  BOOST_CHECK_CLOSE(fwd->out.par[Y], -bwd->out.par[Y], 1e-6);
  BOOST_CHECK_CLOSE(fwd->out.par[TY], bwd->out.par[TY], 1e-6);
  BOOST_CHECK_GT(bwd->out.par[T], 0.);
  BOOST_CHECK_CLOSE(fwd->out.par[T], bwd->out.par[T], 1e-9);
}

// In a non-uniform field the propagator must still be a fourth-order Runge-Kutta: halving the step divides the error by
// about 2^4 = 16. This needs the field at the position of every stage: reusing the field of stage 2 for stage 3 (whose
// position differs by order h^2) makes the scheme second order, with a ratio of 4. The field has a strong gradient
// across the track (it doubles over 500 mm in y), so that the effect is well above rounding; the field gradients are
// neglected only in the Jacobian, not in the state.
BOOST_AUTO_TEST_CASE(fourth_order_in_non_uniform_field) {
  struct gradient_field : field {
    field_vector at(double, double y, double) const override { return {0.6 * (1. + y / 500.), 0., 0.}; }
  } const bf;
  auto const in  = make_state(0., 0., 0.2, 0.1, -1. / 300., 0.);
  auto const ref = propagate(in, 1000., mu, bf, {0.25, 1e3});
  BOOST_REQUIRE(ref);
  double err[3];
  double const steps[3] = {100., 50., 25.};
  for (int i = 0; i < 3; ++i) {
    auto const r = propagate(in, 1000., mu, bf, {steps[i], 1e3});
    BOOST_REQUIRE(r);
    err[i] = std::abs(r->out.par[Y] - ref->out.par[Y]);
  }
  BOOST_TEST_MESSAGE("error on y for steps 100, 50, 25 mm: " << err[0] << ", " << err[1] << ", " << err[2] << " mm");
  BOOST_CHECK_GT(err[0] / err[1], 12.);
  BOOST_CHECK_GT(err[1] / err[2], 12.);
  BOOST_CHECK_LT(err[1] / err[2], 20.);
}

// A straight track (q/p = 0) has 1/beta = sqrt(1 + m^2 (q/p)^2) = 1: the time of flight is the path length over c and
// does not depend on the mass hypothesis.
BOOST_AUTO_TEST_CASE(straight_track_time_independent_of_mass) {
  uniform_field const bf({0.6, 0., 0.});
  auto const in = make_state(0., 0., 0.3, -0.4, 0., 2.);
  auto const rmu = propagate(in, 1000., mu, bf, cfg);
  auto const rpr = propagate(in, 1000., hypothesis_from_pdg(2212), bf, cfg);
  BOOST_REQUIRE(rmu && rpr);
  double const length = 1000. * std::sqrt(1. + 0.3 * 0.3 + 0.4 * 0.4);
  BOOST_CHECK_CLOSE(rmu->path_length, length, 1e-9);
  BOOST_CHECK_CLOSE(rmu->out.par[T], 2. + length / CLHEP::c_light, 1e-9);
  BOOST_CHECK_EQUAL(rmu->out.par[T], rpr->out.par[T]);
  BOOST_CHECK_EQUAL(rmu->F(T, QOP), rpr->F(T, QOP));
}

// Relations of docs/kalman_filter.md, appendix A, between the state and the helix parameters (x, y, 1/R~,
// tan lambda_dip, phi_h) in a uniform field along x, Bx > 0. The helix is
//   z = zc + R cos phi_h,  y = yc + R sin phi_h,  x = x0 + sign(q) R tan(lambda_dip) (phi_h - phi_h0),
// travelled with phi_h increasing for a positive charge. The state built from the helix parameters is propagated with
// the Runge-Kutta propagator to another point of the helix and compared with it; the inverse relations, |v| and the
// path length are checked too. The same q/p gives the same trajectory for a unit charge and for an alpha.
BOOST_AUTO_TEST_CASE(helix_parametrisation) {
  double const B = 0.6, R = 2000., x0 = 100., zc = 300., yc = -50., dphi = 0.3;
  uniform_field const bf({B, 0., 0.});
  for (auto const& hyp : {mu, alpha()}) {
    for (int sq : {+1, -1}) {
      for (double tl : {0.4, -0.7, 0.}) {
        for (double phi0 : {-CLHEP::halfpi + 0.4, CLHEP::halfpi - 0.5, -2.5, 2.2}) {
          double const cl     = 1. / std::sqrt(1. + tl * tl); // cos lambda_dip
          double const inv_Rt = sq / R;                       // signed curvature 1/R~, sign of q for Bx > 0
          // State from the helix parameters
          state in;
          in.z         = zc + R * std::cos(phi0);
          in.par[X]    = x0;
          in.par[Y]    = yc + R * std::sin(phi0);
          in.par[TX]   = -sq * tl / std::sin(phi0);
          in.par[TY]   = -std::cos(phi0) / std::sin(phi0);
          in.par[QOP]  = cl * inv_Rt / (kappa * B);
          in.par[T]    = 0.;
          in.direction = -sq * std::sin(phi0) > 0 ? +1 : -1;
          // p_perp = kappa |q| B R and p = p_perp / cos lambda_dip
          BOOST_CHECK_CLOSE(momentum(in, hyp), kappa * hyp.abs_charge * B * R / cl, 1e-10);
          // |v| = 1 / (cos lambda_dip |sin phi_h|)
          double const nv = std::sqrt(1. + in.par[TX] * in.par[TX] + in.par[TY] * in.par[TY]);
          BOOST_CHECK_CLOSE(nv, 1. / (cl * std::abs(std::sin(phi0))), 1e-10);
          // Propagate to the point of the helix at phi_h1
          double const phi1 = phi0 + sq * dphi;
          double const z1   = zc + R * std::cos(phi1);
          auto const res    = propagate(in, z1, hyp, bf, {5., 1e3});
          BOOST_REQUIRE(res);
          auto const& o = res->out.par;
          BOOST_CHECK_SMALL(o[X] - (x0 + R * tl * dphi), 1e-5); // 10 nm: Runge-Kutta truncation with 5 mm steps
          BOOST_CHECK_SMALL(o[Y] - (yc + R * std::sin(phi1)), 1e-5);
          BOOST_CHECK_SMALL(o[TX] - (-sq * tl / std::sin(phi1)), 1e-8);
          BOOST_CHECK_SMALL(o[TY] - (-std::cos(phi1) / std::sin(phi1)), 1e-8);
          BOOST_CHECK_CLOSE(res->path_length, R * dphi / cl, 1e-2); // trapezoidal rule on |v|: 1e-4 relative
          // Inverse relations on the propagated state: sigma, tan lambda_dip, phi_h, 1/R~
          int const sg = res->out.direction;
          BOOST_CHECK_EQUAL(sg, in.direction);
          int const s_rot = o[QOP] * B > 0 ? +1 : -1;
          BOOST_CHECK_EQUAL(s_rot, sq);
          double const tl_back  = sg * o[TX] / std::sqrt(1. + o[TY] * o[TY]);
          double const phi_back = std::atan2(-s_rot * sg, s_rot * sg * o[TY]);
          double const cl_back  = 1. / std::sqrt(1. + tl_back * tl_back);
          BOOST_CHECK_SMALL(tl_back - tl, 1e-8);
          BOOST_CHECK_SMALL(std::remainder(phi_back - phi1, CLHEP::twopi), 1e-8);
          BOOST_CHECK_CLOSE(o[QOP] * kappa * B / cl_back, inv_Rt, 1e-6);
        }
      }
    }
  }
}

// The particle hypothesis must have a positive |q| (it divides the mass) and a mass that is not negative.
BOOST_AUTO_TEST_CASE(hypothesis_is_checked) {
  uniform_field const bf({0.6, 0., 0.});
  auto const in = make_state(0., 0., 0.1, 0.1, -1. / 1000., 0.);
  BOOST_CHECK_THROW(propagate(in, 100., particle_hypothesis{mu.mass, 0.}, bf, cfg), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., particle_hypothesis{mu.mass, -1.}, bf, cfg), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., particle_hypothesis{-1., 1.}, bf, cfg), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., particle_hypothesis{std::nan(""), 1.}, bf, cfg), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., particle_hypothesis{mu.mass, std::nan("")}, bf, cfg), std::invalid_argument);
  BOOST_CHECK(propagate(in, 100., particle_hypothesis{0., 1.}, bf, cfg));
}

// The settings have no defaults: a value-initialised config, or a non-positive value, is rejected.
BOOST_AUTO_TEST_CASE(settings_are_required) {
  uniform_field const bf({0.6, 0., 0.});
  auto const in = make_state(0., 0., 0.1, 0.1, -1. / 1000., 0.);
  BOOST_CHECK_THROW(propagate(in, 100., mu, bf, propagator_config{}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., mu, bf, {0., 1e3}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., mu, bf, {10., -1.}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., mu, bf, {std::nan(""), 1e3}), std::invalid_argument);
  BOOST_CHECK(propagate(in, 100., mu, bf, cfg));
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
