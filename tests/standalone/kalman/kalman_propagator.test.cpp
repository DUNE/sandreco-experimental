#define BOOST_TEST_MODULE kalman_propagator
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>
#include <kalman/propagator.hpp>

#include <cmath>

using namespace sand::tracker::kalman;

namespace {
  double constexpr m_mu = 105.6583755; // MeV

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

// A track starting along z in a field along x stays in the yz plane and follows a circle of radius R = p / (kappa B).
// For a negative charge the force q v x B points towards -y.
BOOST_AUTO_TEST_CASE(helix_in_uniform_field) {
  double const p = 1000., B = 0.6, z1 = 2000.;
  uniform_field const bf({B, 0., 0.});
  for (int q : {-1, +1}) {
    auto const in  = make_state(0., 0., 0., 0., q / p, 0.);
    auto const res = propagate(in, z1, m_mu, bf, {10., 1e3});
    BOOST_REQUIRE(res);
    double const R    = p / (s_kappa * B);
    double const root = std::sqrt(R * R - z1 * z1);
    double const y    = q * (R - root);
    double const ty   = q * z1 / root;
    double const s    = R * std::asin(z1 / R);
    double const beta = p / std::sqrt(p * p + m_mu * m_mu);
    BOOST_CHECK_SMALL(res->out.par[X], 1e-9);
    BOOST_CHECK_CLOSE(res->out.par[Y], y, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[TY], ty, 1e-6);
    BOOST_CHECK_CLOSE(res->out.par[T], s / (beta * s_c), 1e-6);
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
  auto const fwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., +1), 1000., m_mu, bf);
  auto const bwd = propagate(make_state(0., 0., 0., 0., -1. / p, 0., -1), -1000., m_mu, bf);
  BOOST_REQUIRE(fwd && bwd);
  BOOST_CHECK_LT(fwd->out.par[Y], 0.);
  BOOST_CHECK_CLOSE(fwd->out.par[Y], -bwd->out.par[Y], 1e-6);
  BOOST_CHECK_CLOSE(fwd->out.par[TY], bwd->out.par[TY], 1e-6);
  BOOST_CHECK_GT(bwd->out.par[T], 0.);
  BOOST_CHECK_CLOSE(fwd->out.par[T], bwd->out.par[T], 1e-9);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
