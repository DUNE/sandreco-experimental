#define BOOST_TEST_MODULE kalman_filter
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>
#include <kalman/filter.hpp>
#include <kalman/measurement.hpp>
#include <kalman/propagator.hpp>

#include <cmath>
#include <random>
#include <vector>

using namespace sand::tracker::kalman;

namespace {
  double constexpr m_mu = 105.6583755; // MeV
  double constexpr pi   = 3.14159265358979323846;

  /// A wire through (x0, y0) at plane z, at angle theta from the x axis, 4 m long, readout end at negative x.
  wire_geometry make_wire(double z, double theta, double x0, double y0) {
    double const ux = std::cos(theta), uy = std::sin(theta), h = 2000.;
    return {{x0 - h * ux, y0 - h * uy, z}, {x0 + h * ux, y0 + h * uy, z}};
  }

  /// Detector toy: planes every 10 mm in groups of three views at 0, +5 and -5 degrees, 1.2 m in z.
  std::vector<double> plane_z() {
    std::vector<double> z;
    for (int i = 0; i < 120; ++i) {
      z.push_back(100. + 10. * i);
    }
    return z;
  }
  double plane_angle(int i) {
    static double const a[3] = {0., 5. * pi / 180., -5. * pi / 180.};
    return a[i % 3];
  }

  struct toy_hit {
    wire_geometry wire;
    double tdc;
    int side;
  };

  /// Truth track, generated with the same propagator (no material): the test checks the filter, not the physics.
  std::vector<toy_hit> make_hits(const state& truth, const field& bf, const reco_calibration& cal, double pitch,
                                 std::mt19937& rng) {
    std::normal_distribution<double> gauss(0., cal.tdc_resolution);
    std::vector<toy_hit> hits;
    auto const zs = plane_z();
    for (size_t i = 0; i < zs.size(); ++i) {
      auto const r    = propagate(truth, zs[i], m_mu, bf);
      double const th = plane_angle(i);
      double const nx = -std::sin(th), ny = std::cos(th);
      double const w  = nx * r->out.par[X] + ny * r->out.par[Y]; // coordinate across the wire
      double const wc = pitch * std::round(w / pitch);           // nearest wire
      // Wire centre on the plane: the point with coordinate wc across and 0 along the wire
      auto const wire = make_wire(zs[i], th, nx * wc, ny * wc);
      double d, L;
      detail::wire_distance(r->out.par, wire, d, L);
      int const side = d >= 0 ? +1 : -1;
      auto const p   = wire_time(r->out, wire, side, cal);
      hits.push_back({wire, p.value + gauss(rng), side});
    }
    return hits;
  }

  /// Forward filter over the hits with the time model and the true side; returns the steps for the smoother.
  std::vector<filter_step> forward(const state& seed, const std::vector<toy_hit>& hits, const field& bf,
                                   const reco_calibration& cal) {
    std::vector<filter_step> steps;
    state cur = seed;
    for (auto const& h : hits) {
      auto const pr = propagate(cur, h.wire.z(), m_mu, bf);
      BOOST_REQUIRE(pr);
      auto const pred = wire_time(pr->out, h.wire, h.side, cal);
      auto const up   = update(pr->out, pred, h.tdc, cal.tdc_resolution * cal.tdc_resolution);
      steps.push_back({pr->out, up.out, pr->F});
      cur = up.out;
    }
    return steps;
  }

  sym_matrix_t seed_cov() {
    sym_matrix_t c;
    c(X, X)     = 100.;
    c(Y, Y)     = 100.;
    c(TX, TX)   = 0.01;
    c(TY, TY)   = 0.01;
    c(QOP, QOP) = std::pow(0.5 / 1000., 2);
    c(T, T)     = 2500.;
    return c;
  }
} // namespace

BOOST_AUTO_TEST_SUITE(measurement)

BOOST_AUTO_TEST_CASE(distance_to_wire) {
  // Wire along x at y = 10 on plane z = 0; track along z through (0, 12): distance +2 across the wire.
  wire_geometry const w = make_wire(0., 0., 0., 10.);
  state s;
  s.par[X] = 0.;
  s.par[Y] = 12.;
  double d, L;
  detail::wire_distance(s.par, w, d, L);
  BOOST_CHECK_CLOSE(d, 2., 1e-9);
  BOOST_CHECK_CLOSE(L, 2000., 1e-9);
  // Slope across the wire shortens the distance by 1/sqrt(1 + ty^2)
  s.par[TY] = 0.5;
  detail::wire_distance(s.par, w, d, L);
  BOOST_CHECK_CLOSE(d, 2. / std::sqrt(1.25), 1e-9);
}

BOOST_AUTO_TEST_CASE(time_derivatives) {
  reco_calibration const cal;
  wire_geometry const w = make_wire(0., 5. * pi / 180., 30., -40.);
  state s;
  s.par[X]   = 400.;
  s.par[Y]   = -3.;
  s.par[TX]  = 0.2;
  s.par[TY]  = -0.4;
  s.par[QOP] = 1. / 500.;
  s.par[T]   = 12.;
  for (int side : {+1, -1}) {
    auto const p = wire_time(s, w, side, cal);
    BOOST_CHECK_CLOSE(p.H[T], 1., 1e-6);
    BOOST_CHECK_SMALL(p.H[QOP], 1e-9);
    // A shift of the track across the wire changes the time by 1/v_drift per mm, with the sign of the side. The
    // distance along the wire also moves slightly with a sloped track, which adds a term of order 1e-5 (1/v_wire).
    double const nx = -std::sin(5. * pi / 180.), ny = std::cos(5. * pi / 180.);
    double const tn = nx * s.par[TX] + ny * s.par[TY];
    double const dd = 1. / std::sqrt(1. + tn * tn);
    BOOST_CHECK_CLOSE(nx * p.H[X] + ny * p.H[Y], side * dd / cal.drift_velocity, 1e-2);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(filter)

// Fit toy tracks with the TDC model: the fitted parameters on the last plane must have pulls with zero mean and unit
// width, and the smoothed parameters on the first plane as well.
BOOST_AUTO_TEST_CASE(pulls_on_toy_tracks) {
  uniform_field const bf({0.6, 0., 0.});
  reco_calibration const cal;
  std::mt19937 rng(12345);
  std::normal_distribution<double> g(0., 1.);

  int constexpr n_toys = 300;
  double sum[2][s_dim] = {}, sum2[2][s_dim] = {};
  int n_ok = 0;
  for (int k = 0; k < n_toys; ++k) {
    state truth;
    truth.z         = 0.;
    truth.par[X]    = 200. * g(rng);
    truth.par[Y]    = 200. * g(rng);
    truth.par[TX]   = 0.2 * g(rng);
    truth.par[TY]   = 0.2 * g(rng);
    double const p  = 600. + 400. * std::abs(g(rng));
    truth.par[QOP]  = (k % 2 ? 1. : -1.) / p;
    truth.par[T]    = 100. * g(rng);
    auto const hits = make_hits(truth, bf, cal, 10., rng);

    // Seed: truth at the first plane smeared within the seed covariance, iterated three times
    auto const first = propagate(truth, hits.front().wire.z(), m_mu, bf)->out;
    state seed       = first;
    seed.cov         = seed_cov();
    for (unsigned int i = 0; i < s_dim; ++i) {
      seed.par[i] += 0.3 * std::sqrt(seed.cov(i, i)) * g(rng);
    }
    std::vector<filter_step> steps;
    for (int it = 0; it < 3; ++it) {
      steps         = forward(seed, hits, bf, cal);
      auto const sm = smooth(steps);
      BOOST_REQUIRE(sm);
      seed     = sm->front();
      seed.cov = seed_cov();
    }
    auto const sm = smooth(steps);
    BOOST_REQUIRE(sm);
    auto const last_truth  = propagate(truth, hits.back().wire.z(), m_mu, bf)->out;
    state const* fitted[2] = {&steps.back().filtered, &sm->front()};
    state const* truths[2] = {&last_truth, &first};
    for (int w = 0; w < 2; ++w) {
      for (unsigned int i = 0; i < s_dim; ++i) {
        double const pull = (fitted[w]->par[i] - truths[w]->par[i]) / std::sqrt(fitted[w]->cov(i, i));
        sum[w][i] += pull;
        sum2[w][i] += pull * pull;
      }
    }
    ++n_ok;
  }
  BOOST_REQUIRE_EQUAL(n_ok, n_toys);
  char const* names[s_dim] = {"x", "y", "tx", "ty", "q/p", "t"};
  for (int w = 0; w < 2; ++w) {
    for (unsigned int i = 0; i < s_dim; ++i) {
      double const mean = sum[w][i] / n_ok;
      double const rms  = std::sqrt(sum2[w][i] / n_ok - mean * mean);
      BOOST_TEST_MESSAGE((w ? "smoothed first plane " : "filtered last plane ")
                         << names[i] << ": pull mean " << mean << ", width " << rms);
      BOOST_CHECK_SMALL(mean, 0.2);
      BOOST_CHECK_CLOSE(rms, 1., 15.);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
