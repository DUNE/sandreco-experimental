#define BOOST_TEST_MODULE gluckstern_smearing_formulas
#include <boost/test/included/unit_test.hpp>

#include <processes/common/gluckstern_smearing/gluckstern_smearing.hpp>
#include <test_helpers.hpp>

// Pure-formula tests for REFACTOR.md §3.2: no root_tgeomanager, no edep_reader, no RNG.
// Inputs are physically plausible synthetic values (not chosen for round numbers); expected
// values are computed independently from the formulas in REFACTOR.md §1, not from the code
// under test.

namespace {
  double constexpr sigma_t   = 0.0002; // m (0.2 mm intrinsic position resolution, transverse)
  double constexpr sigma_l   = 0.002;  // m (2 mm intrinsic position resolution, longitudinal)
  double constexpr b_field   = 0.6;    // T
  double constexpr lever_arm = 1.5;    // m
  int constexpr n_hits       = 10;
  double constexpr p_t       = 0.5; // GeV/c
  double constexpr p         = 0.6; // GeV/c, total momentum (> p_t, some dip angle)

  double constexpr x_over_x0_full       = 0.01;  // full 3D path, in X0 units
  double constexpr x_over_x0_transverse = 0.008; // YZ-plane projection, in X0 units

  sand::common::GlucksternGeometry make_geometry() { return {n_hits, lever_arm, x_over_x0_full, x_over_x0_transverse}; }

  double constexpr tolerance_percent = 1e-6;
} // namespace

BOOST_AUTO_TEST_SUITE(pure_formulas)

BOOST_AUTO_TEST_CASE(pt_measurement_resolution) {
  auto const sigma = sand::common::gluckstern_pt_resolution(p_t, make_geometry(), sigma_t, b_field);
  BOOST_CHECK_CLOSE(sigma, 0.0017707090508657685, tolerance_percent);
}

BOOST_AUTO_TEST_CASE(dip_measurement_resolution) {
  auto const sigma = sand::common::gluckstern_dip_resolution(make_geometry(), sigma_l);
  BOOST_CHECK_CLOSE(sigma, 0.0013211565181516327, tolerance_percent);
}

BOOST_AUTO_TEST_CASE(mcs_angle_resolution_full_path) {
  auto const sigma = sand::common::mcs_angle_resolution(x_over_x0_full, p);
  BOOST_CHECK_CLOSE(sigma, 0.0018700080079802255, tolerance_percent);
}

BOOST_AUTO_TEST_CASE(mcs_angle_resolution_transverse_path) {
  auto const sigma = sand::common::mcs_angle_resolution(x_over_x0_transverse, p);
  BOOST_CHECK_CLOSE(sigma, 0.0016553950315617086, tolerance_percent);
}

BOOST_AUTO_TEST_CASE(pt_mcs_resolution) {
  auto const sigma = sand::common::mcs_pt_resolution(make_geometry(), b_field, p);
  BOOST_CHECK_CLOSE(sigma, 0.006925955585111946, tolerance_percent);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
