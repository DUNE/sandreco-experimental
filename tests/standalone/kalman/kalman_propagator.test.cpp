#define BOOST_TEST_MODULE kalman_propagator
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>
#include <kalman/particle.hpp>
#include <kalman/propagator.hpp>

#include <Geant4/CLHEP/Units/PhysicalConstants.h>
#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <cmath>
#include <stdexcept>

using namespace sand::tracker::kalman;

namespace {
  double const m_mu = mass_from_pdg(13); // MeV

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
    auto const res = propagate(in, z1, m_mu, bf, {10., 1e3});
    BOOST_REQUIRE(res);
    double const R    = p / (kappa * std::abs(q) * B); // p_{perp} = p
    double const root = std::sqrt(R * R - z1 * z1);
    double const y    = q * (R - root);
    double const ty   = q * z1 / root;
    double const s    = R * std::asin(z1 / R);
    double const beta = p / std::sqrt(p * p + m_mu * m_mu);
    BOOST_CHECK_SMALL(res->out.par[X], 1e-9);
    BOOST_CHECK_CLOSE(res->out.par[Y], y, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[TY], ty, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[T], s / (beta * CLHEP::c_light), 1e-6);
    BOOST_CHECK_CLOSE(res->path_length, s, 1e-4);
  }
}

// Propagating forward and back must return to the start, and the two Jacobians must be inverse to each other.
BOOST_AUTO_TEST_CASE(round_trip) {
  uniform_field const bf({0.6, 0.05, -0.02});
  auto const in = make_state(10., -20., 0.3, -0.2, -1. / 700., 5.);
  auto const fw = propagate(in, 1500., m_mu, bf, {20., 1e3});
  BOOST_REQUIRE(fw);
  auto const bw = propagate(fw->out, 0., m_mu, bf, {20., 1e3});
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
    auto const ref = propagate(in, z, m_mu, bf, {10., 1e3});
    BOOST_REQUIRE(ref);
    double const eps[s_dim] = {1e-3, 1e-3, 1e-6, 1e-6, 1e-9, 1e-3};
    for (unsigned int j = 0; j < s_dim; ++j) {
      auto up = in, dn = in;
      up.par[j] += eps[j];
      dn.par[j] -= eps[j];
      auto const ru = propagate(up, z, m_mu, bf, {10., 1e3});
      auto const rd = propagate(dn, z, m_mu, bf, {10., 1e3});
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
  auto const fwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., +1), 1000., m_mu, bf, cfg);
  auto const bwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., -1), -1000., m_mu, bf, cfg);
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
  auto const ref = propagate(in, 1000., m_mu, bf, {0.25, 1e3});
  BOOST_REQUIRE(ref);
  double err[3];
  double const steps[3] = {100., 50., 25.};
  for (int i = 0; i < 3; ++i) {
    auto const r = propagate(in, 1000., m_mu, bf, {steps[i], 1e3});
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
  auto const mu = propagate(in, 1000., m_mu, bf, cfg);
  auto const pr = propagate(in, 1000., mass_from_pdg(2212), bf, cfg);
  BOOST_REQUIRE(mu && pr);
  double const length = 1000. * std::sqrt(1. + 0.3 * 0.3 + 0.4 * 0.4);
  BOOST_CHECK_CLOSE(mu->path_length, length, 1e-9);
  BOOST_CHECK_CLOSE(mu->out.par[T], 2. + length / CLHEP::c_light, 1e-9);
  BOOST_CHECK_EQUAL(mu->out.par[T], pr->out.par[T]);
  BOOST_CHECK_EQUAL(mu->F(T, QOP), pr->F(T, QOP));
}

// The settings have no defaults: a value-initialised config, or a non-positive value, is rejected.
BOOST_AUTO_TEST_CASE(settings_are_required) {
  uniform_field const bf({0.6, 0., 0.});
  auto const in = make_state(0., 0., 0.1, 0.1, -1. / 1000., 0.);
  BOOST_CHECK_THROW(propagate(in, 100., m_mu, bf, propagator_config{}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., m_mu, bf, {0., 1e3}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., m_mu, bf, {10., -1.}), std::invalid_argument);
  BOOST_CHECK_THROW(propagate(in, 100., m_mu, bf, {std::nan(""), 1e3}), std::invalid_argument);
  BOOST_CHECK(propagate(in, 100., m_mu, bf, cfg));
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
