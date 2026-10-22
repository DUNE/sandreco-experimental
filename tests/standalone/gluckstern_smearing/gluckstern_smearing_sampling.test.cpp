#define BOOST_TEST_MODULE gluckstern_smearing_sampling
#include <boost/test/included/unit_test.hpp>

#include <processes/common/gluckstern_smearing/gluckstern_smearing.hpp>
#include <test_helpers.hpp>

#include <ufw/context.hpp>

#include <duneanaobj/StandardRecord/SRLorentzVector.h>
#include <duneanaobj/StandardRecord/SRVector3D.h>

#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace {

  ufw::context& test_context() {
    static ufw::context ctx;
    static bool const initialized = [] {
      ctx.init(ufw::json::parse(R"({"contexts": {"keys": 1}})"));
      ctx.before_run();
      return true;
    }();
    (void)initialized;
    return ctx;
  }

  ::caf::SRLorentzVector muon_momentum(double p, double dip_angle, double zy_angle) {
    double const p_transverse = p * std::cos(dip_angle);
    double constexpr m_mu     = 0.10566; // GeV, PDG muon mass

    ::caf::SRLorentzVector true_p;
    true_p.px = static_cast<float>(p * std::sin(dip_angle));
    true_p.py = static_cast<float>(p_transverse * std::sin(zy_angle));
    true_p.pz = static_cast<float>(p_transverse * std::cos(zy_angle));
    true_p.E  = static_cast<float>(std::hypot(p, m_mu));
    return true_p;
  }

  double constexpr sigma_t   = 0.0002; // m (0.2 mm intrinsic position resolution, transverse)
  double constexpr sigma_l   = 0.002;  // m (2 mm intrinsic position resolution, longitudinal)
  double constexpr b_field   = 0.6;    // T
  double constexpr lever_arm = 1.5;    // m
  int constexpr n_hits       = 10;

  sand::common::GlucksternGeometry well_measured_geometry() { return {n_hits, lever_arm, 0.01, 0.008}; }

  sand::common::GlucksternGeometry degenerate_geometry() { return {2, 0.01, 0.001, 0.0008}; }

  struct sample_stats {
    double mean;
    double stddev;
  };

  sample_stats stats_of(std::vector<double> const& v) {
    BOOST_REQUIRE(!v.empty());
    double const mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    double sq_sum{};
    for (double const x : v) {
      sq_sum += (x - mean) * (x - mean);
    }
    return {mean, std::sqrt(sq_sum / static_cast<double>(v.size()))};
  }

  // Draws n_samples independent smearings of the same true trajectory and checks that the empirical
  // distribution of p_T, dip angle and zy angle matches the sigmas the pure formulas predict for it.
  // Every accepted sample is recovered exactly (not approximately) from the returned SRVector3D: with
  // Y = pt*sin(zy), Z = pt*cos(zy), X = pt*tan(dip), hypot(Y,Z) is pt bit-for-bit and atan(X/hypot(Y,Z))
  // is dip whenever |dip| < pi/2 (true here: dip is only perturbed by a few mrad around a small central
  // value), so no numerical approximation is introduced by the test itself.
  void check_trajectory_statistics(::caf::SRLorentzVector const& true_p, sand::common::GlucksternGeometry const& geom,
                                   double p, double dip, double zy) {
    test_context();

    double const p_transverse = p * std::cos(dip);

    double const expected_pt_res =
        std::hypot(sand::common::gluckstern_pt_resolution(p_transverse, geom, sigma_t, b_field),
                   sand::common::mcs_pt_resolution(geom, b_field, p));
    double const expected_dip_res = std::hypot(sand::common::gluckstern_dip_resolution(geom, sigma_l),
                                               sand::common::mcs_angle_resolution(geom.path_len_over_x0_full, p));
    double const expected_zy_res  = sand::common::mcs_angle_resolution(geom.path_len_over_x0_transverse, p);

    std::size_t constexpr n_samples = 200'000;
    std::vector<double> pt_ratio;
    std::vector<double> dip_residual;
    std::vector<double> zy_residual;
    pt_ratio.reserve(n_samples);
    dip_residual.reserve(n_samples);
    zy_residual.reserve(n_samples);

    int n_nullopt{};
    for (std::size_t i{}; i != n_samples; ++i) {
      auto const smeared = sand::common::smear_momentum_gluckstern(true_p, geom, sigma_t, sigma_l, b_field);
      if (!smeared) {
        ++n_nullopt;
        continue;
      }

      double const smeared_pt  = std::hypot(smeared->y, smeared->z); // == smeared_p_transverse, exactly
      double const smeared_dip = std::atan(smeared->x / smeared_pt); // == smeared_dip_angle for |dip|<pi/2
      double const smeared_zy  = std::atan2(smeared->y, smeared->z); // == smeared_zy_angle for |zy|<pi

      BOOST_REQUIRE(std::isfinite(smeared->Mag()));
      BOOST_REQUIRE_GT(smeared->Mag(), 0.);

      pt_ratio.push_back(smeared_pt / p_transverse);
      dip_residual.push_back(smeared_dip - dip);
      zy_residual.push_back(smeared_zy - zy);
    }

    BOOST_TEST_MESSAGE("n_nullopt = " << n_nullopt << " / " << n_samples);
    BOOST_REQUIRE_EQUAL(n_nullopt, 0);

    auto const pt_stats  = stats_of(pt_ratio);
    auto const dip_stats = stats_of(dip_residual);
    auto const zy_stats  = stats_of(zy_residual);

    BOOST_CHECK_SMALL(pt_stats.mean - 1.0, 10. * expected_pt_res / std::sqrt(static_cast<double>(n_samples)));
    BOOST_CHECK_SMALL(dip_stats.mean, 10. * expected_dip_res / std::sqrt(static_cast<double>(n_samples)));
    BOOST_CHECK_SMALL(zy_stats.mean, 10. * expected_zy_res / std::sqrt(static_cast<double>(n_samples)));

    BOOST_CHECK_CLOSE(pt_stats.stddev, expected_pt_res, 5.);
    BOOST_CHECK_CLOSE(dip_stats.stddev, expected_dip_res, 5.);
    BOOST_CHECK_CLOSE(zy_stats.stddev, expected_zy_res, 5.);
  }

} // namespace

BOOST_AUTO_TEST_SUITE(sampling)

BOOST_AUTO_TEST_CASE(well_measured_forward_muon) {
  double const p   = 0.8;  // GeV/c
  double const dip = 0.05; // rad, ~2.9 deg out of the curvature plane
  double const zy  = 0.2;  // rad, azimuth within the curvature plane

  check_trajectory_statistics(muon_momentum(p, dip, zy), well_measured_geometry(), p, dip, zy);
}

BOOST_AUTO_TEST_CASE(well_measured_steep_muon) {
  double const p   = 1.5;  // GeV/c
  double const dip = -0.3; // rad, ~17 deg, opposite sign
  double const zy  = 1.0;  // rad

  check_trajectory_statistics(muon_momentum(p, dip, zy), well_measured_geometry(), p, dip, zy);
}

BOOST_AUTO_TEST_CASE(degenerate_geometry_is_left_unsmeared) {
  test_context();

  auto const true_p = muon_momentum(0.5, 0.1, 0.1);
  auto const geom   = degenerate_geometry();

  for (int i{}; i != 10; ++i) {
    auto const smeared = sand::common::smear_momentum_gluckstern(true_p, geom, sigma_t, sigma_l, b_field);
    BOOST_CHECK(!smeared.has_value());
  }
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
