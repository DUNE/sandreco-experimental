#define BOOST_TEST_MODULE kalman_filter
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>
#include <kalman/filter.hpp>
#include <kalman/measurement.hpp>
#include <kalman/particle.hpp>
#include <kalman/propagator.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <TDatabasePDG.h>
#include <TParticlePDG.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using namespace sand::tracker::kalman;

namespace {
  particle_hypothesis const mu = hypothesis_from_pdg(13); // muon: mass from the table, |q| = 1
  double constexpr pi = CLHEP::pi;

  /// Calibration and Runge-Kutta settings of the toy detector: the library has no defaults, the tests set them.
  reco_calibration const toy_cal{0.05, 200., 3.5, 0.};
  propagator_config const toy_prop{50., 1e3};

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
    std::normal_distribution<double> gauss(0., cal.time_resolution);
    std::vector<toy_hit> hits;
    auto const zs = plane_z();
    for (size_t i = 0; i < zs.size(); ++i) {
      auto const r    = propagate(truth, zs[i], mu, bf, toy_prop);
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
      auto const pr = propagate(cur, h.wire.z(), mu, bf, toy_prop);
      BOOST_REQUIRE(pr);
      auto const pred = wire_time(pr->out, h.wire, h.side, cal);
      auto const up   = update(pr->out, pred, h.tdc, cal.time_resolution * cal.time_resolution);
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
  // A slope across the wire shortens the distance by a factor 1/sqrt(1 + t_n^2), here with t_n = ty
  s.par[TY] = 0.5;
  detail::wire_distance(s.par, w, d, L);
  BOOST_CHECK_CLOSE(d, 2. / std::sqrt(1.25), 1e-9);
}

BOOST_AUTO_TEST_CASE(time_derivatives) {
  reco_calibration const& cal = toy_cal;
  wire_geometry const w       = make_wire(0., 5. * pi / 180., 30., -40.);
  state s;
  s.par[X]   = 400.;
  s.par[Y]   = -3.;
  s.par[TX]  = 0.2;
  s.par[TY]  = -0.4;
  s.par[QOP] = 1. / 500.;
  s.par[T]   = 12.;
  for (int side : {+1, -1}) {
    auto const p = wire_time(s, w, side, cal);
    BOOST_CHECK_EQUAL(p.H[T], 1.);
    BOOST_CHECK_EQUAL(p.H[QOP], 0.);
    // A shift of the track across the wire changes the time by 1/v_d per mm, with the sign of the side. The
    // distance along the wire also moves slightly with a sloped track, which adds a term of order 1e-5 (1/v_w).
    double const nx = -std::sin(5. * pi / 180.), ny = std::cos(5. * pi / 180.);
    double const tn = nx * s.par[TX] + ny * s.par[TY];
    double const dd = 1. / std::sqrt(1. + tn * tn);
    BOOST_CHECK_CLOSE(nx * p.H[X] + ny * p.H[Y], side * dd / cal.drift_velocity, 1e-2);
  }
}

// The analytic derivative row of wire_time must agree with central differences of the same side-fixed model, for
// many random states and wires: straight (along x or y) and stereo, both sides, to a relative tolerance of 1e-6.
// The finite-difference steps live only here. The central differences are Richardson-extrapolated (steps h and h/2),
// which removes the h^2 truncation term; what is left is the rounding of the model value (about 100 ns) divided by the
// step, which is added to the tolerance. It only matters for derivatives close to zero.
BOOST_AUTO_TEST_CASE(time_derivatives_vs_finite_differences) {
  reco_calibration const cal{0.05, 200., 3.5, 7.};
  double const step[s_dim] = {1e-2, 1e-2, 1e-3, 1e-3, 1e-7, 1e-2};
  double const angles[]    = {0., pi / 2., 5. * pi / 180., -5. * pi / 180., 0.3, 2.};
  std::mt19937 rng(4242);
  std::uniform_real_distribution<double> uni(-1., 1.);

  auto model = [&](const vector_t& par, const wire_geometry& w, int side) {
    double d, L;
    detail::wire_distance(par, w, d, L);
    return par[T] + L / cal.wire_velocity + side * d / cal.drift_velocity + cal.t0_offset;
  };
  auto central = [&](const state& s, const wire_geometry& w, int side, unsigned int i, double h) {
    vector_t up = s.par, dn = s.par;
    up[i] += h;
    dn[i] -= h;
    return (model(up, w, side) - model(dn, w, side)) / (2. * h);
  };

  int n_checked = 0, n_rounding = 0;
  double worst = 0.;
  for (int k = 0; k < 2000; ++k) {
    double const th = angles[k % 6];
    double const ux = std::cos(th), uy = std::sin(th);
    double const nx = -uy, ny = ux;
    double const length = 2500. + 1500. * uni(rng);
    double const z      = 1000. * uni(rng);
    wire_geometry::point const head{2000. * uni(rng), 2000. * uni(rng), z};
    wire_geometry const w(head, {head[0] + length * ux, head[1] + length * uy, z});
    // Track within a drift cell of the wire, anywhere along it, with slopes up to 45 degrees
    double const a = 0.5 * length * (1. + uni(rng)), b = 6. * uni(rng);
    state s;
    s.z        = z;
    s.par[X]   = head[0] + a * ux + b * nx;
    s.par[Y]   = head[1] + a * uy + b * ny;
    s.par[TX]  = uni(rng);
    s.par[TY]  = uni(rng);
    s.par[QOP] = uni(rng) / 500.;
    s.par[T]   = 100. * uni(rng);
    for (int side : {+1, -1}) {
      auto const p = wire_time(s, w, side, cal);
      BOOST_CHECK_CLOSE(p.value, model(s.par, w, side), 1e-12);
      for (unsigned int i = 0; i < s_dim; ++i) {
        double const h        = step[i];
        double const fd       = (4. * central(s, w, side, i, 0.5 * h) - central(s, w, side, i, h)) / 3.;
        double const rounding = 16. * std::numeric_limits<double>::epsilon() * std::abs(p.value) / h;
        double const relative = 1e-6 * std::max(std::abs(fd), std::abs(p.H[i]));
        double const diff     = std::abs(p.H[i] - fd);
        if (i == QOP) {
          // q/p does not enter the model: both are exactly zero
          BOOST_CHECK_EQUAL(p.H[i], 0.);
          BOOST_CHECK_EQUAL(fd, 0.);
          continue;
        }
        if (rounding > relative) {
          ++n_rounding;
        } else {
          worst = std::max(worst, diff / std::max(std::abs(fd), std::abs(p.H[i])));
        }
        BOOST_CHECK_MESSAGE(diff <= relative + rounding, "angle " << th << " side " << side << " H[" << i
                                                                  << "] = " << p.H[i] << " vs finite differences " << fd
                                                                  << ", difference " << diff);
        ++n_checked;
      }
    }
  }
  BOOST_TEST_MESSAGE("analytic H vs central differences: " << n_checked << " derivatives (q/p excluded), worst "
                                                           << "relative difference " << worst << "; " << n_rounding
                                                           << " derivatives close to zero, within the rounding bound");
}

// The constructor of wire_geometry checks that the wire is in a plane of constant z and has a length.
BOOST_AUTO_TEST_CASE(wire_geometry_checks) {
  double const tol = s_wire_plane_tolerance;
  wire_geometry const ok({-100., 5., 300.}, {100., 5., 300. + 0.5 * tol});
  BOOST_CHECK_CLOSE(ok.length(), 200., 1e-12);
  BOOST_CHECK_CLOSE(ok.z(), 300. + 0.25 * tol, 1e-12);
  double ux, uy;
  ok.along(ux, uy);
  BOOST_CHECK_CLOSE(ux, 1., 1e-12);
  BOOST_CHECK_SMALL(uy, 1e-15);
  BOOST_CHECK_EQUAL(ok.head()[0], -100.);
  BOOST_CHECK_EQUAL(ok.tail()[0], 100.);
  // Ends at different z, beyond the tolerance, in either order
  BOOST_CHECK_THROW(wire_geometry({-100., 5., 300.}, {100., 5., 300. + 2. * tol}), std::invalid_argument);
  BOOST_CHECK_THROW(wire_geometry({-100., 5., 300. + 2. * tol}, {100., 5., 300.}), std::invalid_argument);
  BOOST_CHECK_THROW(wire_geometry({0., 0., 0.}, {0., 0., 10.}), std::invalid_argument);
  // Zero length
  BOOST_CHECK_THROW(wire_geometry({1., 2., 3.}, {1., 2., 3.}), std::invalid_argument);
  // NaN coordinates are rejected
  BOOST_CHECK_THROW(wire_geometry({0., 0., std::nan("")}, {1., 0., 0.}), std::invalid_argument);
  BOOST_CHECK_THROW(wire_geometry({std::nan(""), 0., 0.}, {1., 0., 0.}), std::invalid_argument);
}

// Masses and charges come from the particle table, once per code; the table gives the charge in units of |e|/3.
BOOST_AUTO_TEST_CASE(hypotheses_from_pdg) {
  double const m_mu_table = TDatabasePDG::Instance()->GetParticle(13)->Mass() * CLHEP::GeV;
  BOOST_CHECK_EQUAL(hypothesis_from_pdg(13).mass, m_mu_table);
  BOOST_CHECK_EQUAL(hypothesis_from_pdg(13).mass, m_mu_table); // second call, from the cache
  BOOST_CHECK_EQUAL(hypothesis_from_pdg(-13).mass, m_mu_table);
  BOOST_CHECK_GT(hypothesis_from_pdg(2212).mass, hypothesis_from_pdg(211).mass);
  BOOST_CHECK_GT(hypothesis_from_pdg(211).mass, hypothesis_from_pdg(13).mass);
  BOOST_CHECK_GT(hypothesis_from_pdg(13).mass, hypothesis_from_pdg(11).mass);
  for (int pdg : {11, -11, 13, -13, 211, -211, 321, 2212, -2212}) {
    BOOST_CHECK_EQUAL(hypothesis_from_pdg(pdg).abs_charge, 1.);
  }
  BOOST_CHECK_THROW(hypothesis_from_pdg(999999), std::invalid_argument);
  // Neutral particles cannot be fitted
  for (int pdg : {22, 2112, 111, 14}) {
    BOOST_CHECK_THROW(hypothesis_from_pdg(pdg), std::invalid_argument);
  }
}

// wire_position is exactly linear in the state (docs/kalman_filter.md, section 2.5): its row H does not depend on the
// state, and h(x + dx) = h(x) + H dx for any dx, also for large changes of the position and of the slopes.
BOOST_AUTO_TEST_CASE(wire_position_is_linear) {
  std::mt19937 rng(99);
  std::uniform_real_distribution<double> u(-1., 1.);
  for (int n = 0; n < 100; ++n) {
    auto const wire = make_wire(500., pi * u(rng), 100. * u(rng), 100. * u(rng));
    state a, b;
    a.z = b.z = wire.z();
    vector_t const scale{500., 500., 2., 2., 1e-3, 100.};
    for (unsigned int i = 0; i < s_dim; ++i) {
      a.par[i] = scale[i] * u(rng);
      b.par[i] = scale[i] * u(rng);
    }
    auto const pa = wire_position(a, wire);
    auto const pb = wire_position(b, wire);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(pa.H[i], pb.H[i]);
    }
    double const linear = pa.value + ROOT::Math::Dot(pa.H, b.par - a.par);
    BOOST_CHECK_SMALL(pb.value - linear, 1e-9 * scale[X]);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(filter)

// The weight must be positive: weight 0 would make V / w infinite and the covariance NaN, a negative weight a negative
// variance. Both, and NaN, throw instead of returning a corrupted state; a small positive weight is accepted and pulls
// the state less than weight 1, a weight above 1 is accepted and pulls it more (V / w < V).
BOOST_AUTO_TEST_CASE(update_rejects_non_positive_weight) {
  state s;
  for (unsigned int i = 0; i < s_dim; ++i) {
    s.cov(i, i) = 1.;
  }
  prediction p;
  p.H[X] = 1.;
  p.H[T] = 1.;
  for (double w : {0., -0.0, -0.5, std::nan(""), -std::numeric_limits<double>::infinity()}) {
    BOOST_CHECK_THROW(update(s, p, 2., 1., w), std::invalid_argument);
  }
  auto const full  = update(s, p, 2., 1.); // default weight: a normal measurement
  auto const one   = update(s, p, 2., 1., 1.);
  auto const small = update(s, p, 2., 1., 1e-3);
  BOOST_CHECK_EQUAL(full.out.par[X], one.out.par[X]);
  BOOST_CHECK_EQUAL(full.out.cov(X, X), one.out.cov(X, X));
  BOOST_CHECK_GT(small.out.par[X], 0.);
  BOOST_CHECK_LT(small.out.par[X], one.out.par[X]);
  auto const big = update(s, p, 2., 1., 4.);
  BOOST_CHECK_GT(big.out.par[X], one.out.par[X]);
  BOOST_CHECK_LT(big.out.cov(X, X), one.out.cov(X, X));
  BOOST_CHECK_EQUAL(big.chi2, one.chi2);
  for (unsigned int i = 0; i < s_dim; ++i) {
    BOOST_CHECK(std::isfinite(small.out.par[i]));
    for (unsigned int j = 0; j < s_dim; ++j) {
      BOOST_CHECK(std::isfinite(small.out.cov(i, j)));
    }
  }
  // The chi2 increment uses the unweighted variance, so it does not depend on the weight
  BOOST_CHECK_EQUAL(small.chi2, one.chi2);
}

// For the optimal gain the Joseph form computed by update() equals the short form (I - K H) C and the information form
// [C^-1 + H^T (V/w)^-1 H]^-1 (docs/kalman_filter.md, appendix B), with and without a weight.
BOOST_AUTO_TEST_CASE(covariance_forms_agree) {
  std::mt19937 rng(777);
  std::uniform_real_distribution<double> u(-1., 1.);
  for (int n = 0; n < 100; ++n) {
    // Random positive definite covariance M M^T + diagonal
    matrix_t M;
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        M(i, j) = u(rng);
      }
    }
    matrix_t const MMt = M * ROOT::Math::Transpose(M);
    state s;
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j <= i; ++j) {
        s.cov(i, j) = MMt(i, j) + (i == j ? 0.1 : 0.);
      }
    }
    prediction p;
    for (unsigned int i = 0; i < s_dim; ++i) {
      p.H[i] = u(rng);
    }
    double const V = 0.5 + 0.5 * u(rng);
    for (double w : {1., 0.3, 2.5}) {
      auto const res = update(s, p, 1., V, w);
      double const v = V / w;
      // Short form
      vector_t const CHt = s.cov * p.H;
      double const S     = ROOT::Math::Dot(p.H, CHt) + v;
      matrix_t IKH(ROOT::Math::SMatrixIdentity{});
      for (unsigned int i = 0; i < s_dim; ++i) {
        for (unsigned int j = 0; j < s_dim; ++j) {
          IKH(i, j) -= CHt[i] / S * p.H[j];
        }
      }
      matrix_t const C_short = IKH * s.cov;
      // Information form
      sym_matrix_t info = s.cov;
      BOOST_REQUIRE(info.Invert());
      for (unsigned int i = 0; i < s_dim; ++i) {
        for (unsigned int j = 0; j <= i; ++j) {
          info(i, j) += p.H[i] * p.H[j] / v;
        }
      }
      BOOST_REQUIRE(info.Invert());
      for (unsigned int i = 0; i < s_dim; ++i) {
        for (unsigned int j = 0; j < s_dim; ++j) {
          double const scale = std::sqrt(res.out.cov(i, i) * res.out.cov(j, j));
          BOOST_CHECK_SMALL((res.out.cov(i, j) - C_short(i, j)) / scale, 1e-9);
          BOOST_CHECK_SMALL((res.out.cov(i, j) - info(i, j)) / scale, 1e-9);
        }
      }
    }
  }
}

// detail::symmetrize() returns (C + C^T) / 2 exactly, and an exactly symmetric matrix unchanged, bit for bit
// (docs/kalman_filter.md, section 2.8).
BOOST_AUTO_TEST_CASE(symmetrize_takes_symmetric_part) {
  std::mt19937 rng(4242);
  std::uniform_real_distribution<double> u(-1., 1.);
  for (int n = 0; n < 100; ++n) {
    matrix_t A;
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        A(i, j) = u(rng);
      }
    }
    sym_matrix_t const S = detail::symmetrize(A);
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        BOOST_CHECK_EQUAL(S(i, j), 0.5 * (A(i, j) + A(j, i)));
      }
    }
    matrix_t const sym   = A + ROOT::Math::Transpose(A); // exactly symmetric
    sym_matrix_t const T = detail::symmetrize(sym);
    for (unsigned int i = 0; i < s_dim; ++i) {
      for (unsigned int j = 0; j < s_dim; ++j) {
        BOOST_CHECK_EQUAL(T(i, j), sym(i, j));
      }
    }
  }
}

// Fit toy tracks with the TDC model: the fitted parameters on the last plane must have pulls with zero mean and unit
// width, and the smoothed parameters on the first plane as well.
BOOST_AUTO_TEST_CASE(pulls_on_toy_tracks) {
  uniform_field const bf({0.6, 0., 0.});
  reco_calibration const& cal = toy_cal;
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
    auto const first = propagate(truth, hits.front().wire.z(), mu, bf, toy_prop)->out;
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
    auto const last_truth  = propagate(truth, hits.back().wire.z(), mu, bf, toy_prop)->out;
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
