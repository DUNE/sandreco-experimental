#define BOOST_TEST_MODULE kalman_material
#include <boost/test/included/unit_test.hpp>

#include <kalman/engine.hpp>
#include <kalman/material.hpp>
#include <kalman/propagator.hpp>
#include <test_helpers.hpp>

#include <ufw/config.hpp>

#include <cmath>
#include <random>
#include <vector>

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

  /// Sternheimer parameters of graphite at 2.210 g/cm3 (PDG muon energy loss table, material 87).
  density_effect const graphite_sternheimer{0.2076, 2.9532, -0.0090, 2.4817, 2.8926, 0.14};

  /// Graphite slab of the given thickness centred at z (PDG: X0 = 19.32 cm at 2.21 g/cm3, I = 78 eV).
  layer graphite(double z, double thickness = 4.) {
    return {z, thickness, 193.2, 2.21, 0.49955, 78e-6, graphite_sternheimer};
  }

  /// Ten 4 mm graphite layers centred at z = sign * (100 i - 50), i = 1..10.
  std::vector<layer> ten_layers(double sign = +1.) {
    std::vector<layer> v;
    for (int i = 1; i <= 10; ++i) {
      v.push_back(graphite(sign * (100. * i - 50.)));
    }
    return v;
  }
} // namespace

BOOST_AUTO_TEST_SUITE(material)

BOOST_AUTO_TEST_CASE(path_length_and_noise) {
  layer const l = graphite(0., 4.);
  BOOST_CHECK_CLOSE(layer_path_length(l, 0.3, -0.4), 4. * std::sqrt(1.25), 1e-12);
  double const th = 1e-3, tx = 0.3, ty = -0.4, n2 = 1.25;
  auto const q = scattering_noise(th, tx, ty);
  BOOST_CHECK_CLOSE(q(TX, TX), th * th * (1. + tx * tx) * n2, 1e-12);
  BOOST_CHECK_CLOSE(q(TY, TY), th * th * (1. + ty * ty) * n2, 1e-12);
  BOOST_CHECK_CLOSE(q(TX, TY), th * th * tx * ty * n2, 1e-12);
  for (unsigned int i = 0; i < s_dim; ++i) {
    for (unsigned int j = 0; j < s_dim; ++j) {
      if ((i == TX || i == TY) && (j == TX || j == TY)) {
        continue;
      }
      BOOST_CHECK_EQUAL(q(i, j), 0.);
    }
  }
}

// Highland: 1 GeV/c muon through 1% of a radiation length.
BOOST_AUTO_TEST_CASE(highland) {
  double const x0 = 193.2;
  BOOST_CHECK_CLOSE(highland_theta0(1000., m_mu, 0.01 * x0, x0), 1.12e-3, 1.);
}

// Bethe-Bloch for a muon in graphite, in MeV cm2/g.
BOOST_AUTO_TEST_CASE(bethe_bloch_graphite) {
  layer const l        = graphite(0.);
  auto per_g           = [&](double p, const layer& m) { return bethe_bloch(p, m_mu, m) * 10. / m.density; };
  layer no_delta       = l;
  no_delta.sternheimer = {};
  // Reference value of the task, 300 MeV/c
  BOOST_CHECK_CLOSE(per_g(300., l), 1.75, 5.);
  // PDG table, ionisation column, at p = 286.8 and 391.7 MeV/c (the table includes small corrections beyond this
  // formula, hence 0.2%)
  BOOST_CHECK_CLOSE(per_g(286.8, l), 1.772, 0.2);
  BOOST_CHECK_CLOSE(per_g(391.7, l), 1.742, 0.2);
  // Without density effect: the same formula evaluated independently, which checks units and kinematics
  BOOST_CHECK_CLOSE(per_g(300., no_delta), 1.83871, 0.01);
}

// Sternheimer delta: zero when not set, continuous at x1, logarithmic above.
BOOST_AUTO_TEST_CASE(density_effect_parametrisation) {
  BOOST_CHECK_EQUAL(density_effect_delta(10., density_effect{}), 0.);
  double const bg1 = std::pow(10., graphite_sternheimer.x1);
  BOOST_CHECK_CLOSE(density_effect_delta(bg1 * (1. - 1e-9), graphite_sternheimer),
                    density_effect_delta(bg1 * (1. + 1e-9), graphite_sternheimer), 1e-4);
  BOOST_CHECK_CLOSE(density_effect_delta(1e4, graphite_sternheimer), 2. * std::log(1e4) - 2.8926, 1e-9);
  BOOST_CHECK_GT(density_effect_delta(0.5, graphite_sternheimer), 0.); // conductor below x0
}

// Half-open interval according to the direction of motion: (lo, hi] for direction +1, [lo, hi) for direction -1,
// layers ordered from s.z towards z_target. A layer on a plane belongs to the segment arriving there along the motion.
BOOST_AUTO_TEST_CASE(fixed_budget_interval) {
  fixed_material_budget const mb({graphite(30.), graphite(10.), graphite(20.), graphite(0.)});
  auto zs = [&](double z_from, double z_to, int dir) {
    state s;
    s.z         = z_from;
    s.direction = dir;
    std::vector<double> out;
    for (auto const& l : mb.crossed_layers(s, z_to)) {
      out.push_back(l.z);
    }
    return out;
  };
  using v = std::vector<double>;
  // Along the motion: starting plane excluded, target plane included
  BOOST_TEST(zs(10., 30., +1) == v({20., 30.}), boost::test_tools::per_element());
  BOOST_TEST(zs(30., 0., -1) == v({20., 10., 0.}), boost::test_tools::per_element());
  // Against the motion: starting plane included, target plane excluded, so the reverse crosses the same layers
  BOOST_TEST(zs(30., 10., +1) == v({30., 20.}), boost::test_tools::per_element());
  BOOST_TEST(zs(0., 30., -1) == v({0., 10., 20.}), boost::test_tools::per_element());
  BOOST_CHECK(zs(30., 30., +1).empty());
  BOOST_CHECK(zs(30., 30., -1).empty());
  BOOST_CHECK(layer_in_interval(30., 10., 30., +1));
  BOOST_CHECK(!layer_in_interval(10., 10., 30., +1));
  BOOST_CHECK(layer_in_interval(10., 10., 30., -1));
  BOOST_CHECK(!layer_in_interval(30., 10., 30., -1));
}

// With layers on both end planes and in between, a propagation and its reverse are inverse to each other, for both
// directions of motion: the state on a plane is downstream of its layer in both passes.
BOOST_AUTO_TEST_CASE(round_trip_with_layers_on_planes) {
  uniform_field const bf({0.6, 0.05, -0.02});
  for (int dir : {+1, -1}) {
    double const z1 = dir * 1000.;
    fixed_material_budget const mb({graphite(0.), graphite(0.5 * z1), graphite(z1)});
    auto const in = make_state(10., -20., 0.2, -0.1, -1. / 500., 0., dir);
    auto const fw = propagate(in, z1, m_mu, bf, mb, {20., 1e3});
    BOOST_REQUIRE(fw);
    auto const bw = propagate(fw->out, 0., m_mu, bf, mb, {20., 1e3});
    BOOST_REQUIRE(bw);
    for (unsigned int i = 0; i < s_dim; ++i) {
      if (i == QOP) {
        BOOST_CHECK_CLOSE(bw->out.par[i], in.par[i], 1e-4); // percent: relative 1e-6
      } else {
        BOOST_CHECK_SMALL(bw->out.par[i] - in.par[i], 1e-7);
      }
    }
    // The layer on the starting plane of the forward pass is not crossed: same result without it
    auto const no_first =
        propagate(in, z1, m_mu, bf, fixed_material_budget({graphite(0.5 * z1), graphite(z1)}), {20., 1e3});
    BOOST_REQUIRE(no_first);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(no_first->out.par[i], fw->out.par[i]);
    }
  }
}

// Forward through several layers and back gives the initial q/p; a backward-moving particle propagated towards
// smaller z loses energy.
BOOST_AUTO_TEST_CASE(energy_loss_direction) {
  uniform_field const bf({0.6, 0., 0.});
  fixed_material_budget const mb(ten_layers());
  auto const in = make_state(10., -20., 0.2, -0.1, -1. / 500., 0.);
  auto const fw = propagate(in, 1100., m_mu, bf, mb);
  BOOST_REQUIRE(fw);
  BOOST_CHECK_GT(std::abs(fw->out.par[QOP]), std::abs(in.par[QOP])); // lost momentum
  BOOST_CHECK_LT(fw->out.par[QOP], 0.);                              // kept the charge sign
  auto const bw = propagate(fw->out, 0., m_mu, bf, mb);
  BOOST_REQUIRE(bw);
  BOOST_CHECK_CLOSE(bw->out.par[QOP], in.par[QOP], 1e-4); // percent: relative 1e-6

  fixed_material_budget const mb_neg(ten_layers(-1.));
  auto const back_in = make_state(0., 0., 0.1, 0.1, 1. / 500., 0., -1);
  auto const back    = propagate(back_in, -1100., m_mu, bf, mb_neg);
  BOOST_REQUIRE(back);
  BOOST_CHECK_GT(std::abs(back->out.par[QOP]), std::abs(back_in.par[QOP]));
  BOOST_CHECK_GT(back->out.par[QOP], 0.);
  // Same particle, propagated against its motion towards larger z: it gains energy.
  auto const against = propagate(back_in, 1100., m_mu, bf, fixed_material_budget(ten_layers()));
  BOOST_REQUIRE(against);
  BOOST_CHECK_LT(std::abs(against->out.par[QOP]), std::abs(back_in.par[QOP]));
}

// A slow muon stops in a thick layer: no result.
BOOST_AUTO_TEST_CASE(stopping_particle) {
  uniform_field const bf({0.6, 0., 0.});
  fixed_material_budget const mb({graphite(50., 100.)});
  BOOST_CHECK(!propagate(make_state(0., 0., 0., 0., 1. / 50., 0.), 200., m_mu, bf, mb));
  BOOST_CHECK(propagate(make_state(0., 0., 0., 0., 1. / 500., 0.), 200., m_mu, bf, mb));
}

// With no layers crossed the result is the field-only one, bit for bit.
BOOST_AUTO_TEST_CASE(empty_budget) {
  uniform_field const bf({0.6, 0.05, -0.02});
  auto in = make_state(10., -20., 0.3, -0.2, -1. / 700., 5.);
  for (unsigned int i = 0; i < s_dim; ++i) {
    in.cov(i, i) = 0.1 * (i + 1);
  }
  in.cov(X, TX) = 0.01;
  fixed_material_budget const empty({});
  fixed_material_budget const outside({graphite(-50.), graphite(0.), graphite(2000.)});
  auto const ref = propagate(in, 1500., m_mu, bf);
  BOOST_REQUIRE(ref);
  for (auto const* mb : {&empty, &outside}) {
    auto const res = propagate(in, 1500., m_mu, bf, *mb);
    BOOST_REQUIRE(res);
    BOOST_CHECK_EQUAL(res->path_length, ref->path_length);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(res->out.par[i], ref->out.par[i]);
      for (unsigned int j = 0; j < s_dim; ++j) {
        BOOST_CHECK_EQUAL(res->out.cov(i, j), ref->out.cov(i, j));
        BOOST_CHECK_EQUAL(res->F(i, j), ref->F(i, j));
      }
    }
  }
}

// Jacobian with field and material against central finite differences, for a particle moving along +z and -z, and
// for propagation along and against the motion.
BOOST_AUTO_TEST_CASE(jacobian_vs_finite_differences) {
  uniform_field const bf({0.6, 0.05, -0.02});
  for (int dir : {+1, -1}) {
    for (int along : {+1, -1}) {
      double const z = dir * along * 1200.;
      fixed_material_budget const mb(ten_layers(dir * along));
      auto const in  = make_state(10., -20., 0.3, -0.2, -1. / 400., 5., dir);
      auto const ref = propagate(in, z, m_mu, bf, mb, {10., 1e3});
      BOOST_REQUIRE(ref);
      double const eps[s_dim] = {1e-3, 1e-3, 1e-6, 1e-6, 1e-9, 1e-3};
      for (unsigned int j = 0; j < s_dim; ++j) {
        auto up = in, dn = in;
        up.par[j] += eps[j];
        dn.par[j] -= eps[j];
        auto const ru = propagate(up, z, m_mu, bf, mb, {10., 1e3});
        auto const rd = propagate(dn, z, m_mu, bf, mb, {10., 1e3});
        BOOST_REQUIRE(ru && rd);
        for (unsigned int i = 0; i < s_dim; ++i) {
          double const fd    = (ru->out.par[i] - rd->out.par[i]) / (2. * eps[j]);
          double const scale = std::max(1., std::abs(fd));
          BOOST_CHECK_MESSAGE(std::abs(ref->F(i, j) - fd) < 1e-4 * scale,
                              "dir " << dir << " along " << along << " F(" << i << "," << j << ") = " << ref->F(i, j)
                                     << " vs " << fd);
        }
      }
    }
  }
}

// Toy Monte Carlo: true tracks scattered at each layer with the Highland angle and the same mean energy loss; the
// library propagates the mean state with its covariance. The pulls at the end must be standard normal.
BOOST_AUTO_TEST_CASE(toy_pulls) {
  uniform_field const bf({0.6, 0., 0.});
  auto const layers = ten_layers();
  fixed_material_budget const mb(layers);
  double const z_end = 1100.;
  auto const in      = make_state(0., 0., 0.1, -0.05, -1. / 1000., 0.);
  auto const mean    = propagate(in, z_end, m_mu, bf, mb);
  BOOST_REQUIRE(mean);

  std::mt19937 rng(4242);
  std::normal_distribution<double> g(0., 1.);
  int const n                = 2000;
  unsigned int const idx[4]  = {X, Y, TX, TY};
  char const* const names[4] = {"x", "y", "tx", "ty"};
  double sum[4] = {0, 0, 0, 0}, sum2[4] = {0, 0, 0, 0};
  for (int k = 0; k < n; ++k) {
    state truth = in;
    for (auto const& l : layers) {
      auto const r = propagate(truth, l.z, m_mu, bf);
      BOOST_REQUIRE(r);
      auto const m = material_step(r->out.par, l, m_mu, +1);
      BOOST_REQUIRE(m);
      truth     = r->out;
      truth.par = m->par;
      // Correlated Gaussian kick on (tx, ty) with covariance Q, by Cholesky decomposition
      double const a = m->noise(TX, TX), b = m->noise(TX, TY), c = m->noise(TY, TY);
      double const l11 = std::sqrt(a), l21 = b / l11, l22 = std::sqrt(c - l21 * l21);
      double const g1 = g(rng), g2 = g(rng);
      truth.par[TX] += l11 * g1;
      truth.par[TY] += l21 * g1 + l22 * g2;
    }
    auto const end = propagate(truth, z_end, m_mu, bf);
    BOOST_REQUIRE(end);
    for (int i = 0; i < 4; ++i) {
      double const pull = (end->out.par[idx[i]] - mean->out.par[idx[i]]) / std::sqrt(mean->out.cov(idx[i], idx[i]));
      sum[i] += pull;
      sum2[i] += pull * pull;
    }
  }
  for (int i = 0; i < 4; ++i) {
    double const mu  = sum[i] / n;
    double const rms = std::sqrt(sum2[i] / n - mu * mu);
    BOOST_TEST_MESSAGE("pull " << names[i] << ": mean " << mu << " width " << rms);
    BOOST_CHECK_SMALL(mu, 0.1);
    BOOST_CHECK_GT(rms, 0.9);
    BOOST_CHECK_LT(rms, 1.1);
  }
}

// engine.propagate with a material budget must be the free propagate with the field and settings of the engine.
BOOST_AUTO_TEST_CASE(engine_propagate_with_material) {
  engine const kf(ufw::json::parse(R"({ "field" : [0.6, 0.05, -0.02], "propagator" : { "max_step" : 20.0 } })"));
  uniform_field const bf({0.6, 0.05, -0.02});
  propagator_config const pc{20., 1e3};
  fixed_material_budget const mb(ten_layers());
  auto in = make_state(10., -20., 0.3, -0.2, -1. / 700., 5.);
  for (unsigned int i = 0; i < s_dim; ++i) {
    in.cov(i, i) = 1e-2 * (i + 1);
  }
  for (double z : {1100., -300.}) {
    auto const a = kf.propagate(in, z, m_mu, mb);
    auto const b = propagate(in, z, m_mu, bf, mb, pc);
    BOOST_REQUIRE(a && b);
    BOOST_CHECK_EQUAL(a->path_length, b->path_length);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(a->out.par[i], b->out.par[i]);
      for (unsigned int j = 0; j < s_dim; ++j) {
        BOOST_CHECK_EQUAL(a->F(i, j), b->F(i, j));
        BOOST_CHECK_EQUAL(a->out.cov(i, j), b->out.cov(i, j));
      }
    }
  }
  // The material is used: through the ten layers the momentum and the covariance differ from the field-only result.
  auto const with    = kf.propagate(in, 1100., m_mu, mb);
  auto const without = kf.propagate(in, 1100., m_mu);
  BOOST_REQUIRE(with && without);
  BOOST_CHECK_GT(std::abs(with->out.par[QOP]), std::abs(without->out.par[QOP]));
  BOOST_CHECK_GT(with->out.cov(TX, TX), without->out.cov(TX, TX));
  // and the step of the engine: with the default step the result differs (Runge-Kutta truncation).
  auto const coarse = propagate(in, 1100., m_mu, bf, mb, {200., 1e3});
  BOOST_REQUIRE(coarse);
  BOOST_CHECK_NE(coarse->out.par[Y], with->out.par[Y]);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
