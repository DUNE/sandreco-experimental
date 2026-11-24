#define BOOST_TEST_MODULE kalman_station_material
#include <boost/test/included/unit_test.hpp>

#include <kalman/engine.hpp>
#include <kalman/material.hpp>
#include <kalman/propagator.hpp>
#include <kalman/station_material.hpp>
#include <test_helpers.hpp>

#include <ufw/config.hpp>

#include <cmath>
#include <exception>

using namespace sand::tracker::kalman;
using sternheimer_parameters = sand::geoinfo::tracker_info::sternheimer_parameters;
using material_layer         = sand::geoinfo::tracker_info::material_layer;

namespace {
  // Porous graphite, G4DensityEffectData index 277 (Sternheimer, Berger and Seltzer 1984, graphite at 1.7 g/cm3).
  // The reference density is the one of the plasma energy of the entry, 26.555 eV: rho = (26.555 / 28.816)^2 / <Z/A>.
  sternheimer_parameters const s_porous_graphite{0.20762, 2.9532, 0.048, 2.5387, 3.155, 0.14, 1.70};
} // namespace

BOOST_AUTO_TEST_SUITE(station_material_tests)

// Index 277 rescaled to the 2.21 g/cm3 of G4_GRAPHITE gives the known parameters.
BOOST_AUTO_TEST_CASE(porous_graphite_to_2_21) {
  auto const d = to_density_effect(s_porous_graphite, 2.21);
  BOOST_CHECK_SMALL(d.a - 0.2076, 5e-5);
  BOOST_CHECK_SMALL(d.k - 2.9532, 5e-5);
  BOOST_CHECK_SMALL(d.x0 - (-0.0090), 5e-5);
  BOOST_CHECK_SMALL(d.x1 - 2.4817, 5e-5);
  BOOST_CHECK_SMALL(d.c_bar - 2.8926, 5e-5);
  BOOST_CHECK_EQUAL(d.delta0, 0.14);
  // exact formulas
  double const r = 2.21 / 1.70;
  BOOST_CHECK_CLOSE(d.c_bar, 3.155 - std::log(r), 1e-12);
  BOOST_CHECK_CLOSE(d.x1, 2.5387 - 0.5 * std::log10(r), 1e-12);
}

// The rescaled delta is the table delta shifted in X = log10(beta gamma) by 0.5 log10 r, in all three ranges.
BOOST_AUTO_TEST_CASE(delta_is_shifted) {
  density_effect const table{0.20762, 2.9532, 0.048, 2.5387, 3.155, 0.14};
  for (double r : {0.5, 1.3, 2.0}) {
    auto const d       = rescale_density_effect(table, r);
    double const shift = 0.5 * std::log10(r);
    for (double x : {-1.0, -0.3, 0.0, 0.5, 1.2, 2.0, 2.7, 3.5}) {
      double const bg = std::pow(10., x);
      double const bt = std::pow(10., x + shift);
      BOOST_CHECK_CLOSE(density_effect_delta(bg, d), density_effect_delta(bt, table), 1e-9);
    }
  }
}

BOOST_AUTO_TEST_CASE(identity_and_round_trip) {
  density_effect const table{0.26142, 2.8697, -0.0178, 2.3415, 2.8680, 0.12};
  auto const same = rescale_density_effect(table, 1.);
  BOOST_CHECK_EQUAL(same.c_bar, table.c_bar);
  BOOST_CHECK_EQUAL(same.x0, table.x0);
  BOOST_CHECK_EQUAL(same.x1, table.x1);
  auto const back = rescale_density_effect(rescale_density_effect(table, 0.73), 1. / 0.73);
  BOOST_CHECK_CLOSE(back.c_bar, table.c_bar, 1e-12);
  BOOST_CHECK_CLOSE(back.x0, table.x0, 1e-10);
  BOOST_CHECK_CLOSE(back.x1, table.x1, 1e-12);
  // Disabled parameters stay disabled
  BOOST_CHECK(!rescale_density_effect(density_effect{}, 2.).enabled());
  BOOST_CHECK_THROW(rescale_density_effect(table, 0.), std::exception);
  BOOST_CHECK_THROW(rescale_density_effect(table, -1.), std::exception);
  sternheimer_parameters bad = s_porous_graphite;
  bad.reference_density      = 0.;
  BOOST_CHECK_THROW(to_density_effect(bad, 2.21), std::exception);
}

BOOST_AUTO_TEST_CASE(layer_conversion) {
  material_layer m{100., 104., 191.5, 2.23, 0.49955, 78e-6, "Graphite", std::nullopt};
  auto l = to_layer(m);
  BOOST_CHECK_EQUAL(l.z, 102.);
  BOOST_CHECK_EQUAL(l.thickness, 4.);
  BOOST_CHECK_EQUAL(l.radiation_length, 191.5);
  BOOST_CHECK_EQUAL(l.density, 2.23);
  BOOST_CHECK_EQUAL(l.z_over_a, 0.49955);
  BOOST_CHECK_EQUAL(l.mean_excitation, 78e-6);
  BOOST_CHECK(!l.sternheimer.enabled());
  m.sternheimer = s_porous_graphite;
  l             = to_layer(m);
  BOOST_CHECK(l.sternheimer.enabled());
  BOOST_CHECK_CLOSE(l.sternheimer.c_bar, 3.155 - std::log(2.23 / 1.70), 1e-12);
}

// "material": default "none", invalid values rejected. "tracker_info" needs a framework context: see the framework
// tests 510-512.
BOOST_AUTO_TEST_CASE(engine_material_key) {
  engine const none(ufw::json::parse(R"({ "field" : [0.6, 0.0, 0.0] })"));
  BOOST_CHECK(none.material() == nullptr);
  engine const explicit_none(ufw::json::parse(R"({ "field" : [0.6, 0.0, 0.0], "material" : "none" })"));
  BOOST_CHECK(explicit_none.material() == nullptr);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, 0, 0], "material" : "geometry" })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, 0, 0], "material" : 1 })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, 0, 0], "material" : { "source" : "none" } })")),
                    std::exception);
}

// The extent of a layer is tested at the position propagated in the field, not on the straight line from the start.
// Uniform 0.6 T field along x: a 1 GeV/c muon starting at y = 0 with ty = 0 is about 90 mm away from y = 0 at
// z = 1000 mm, while the straight line stays at y = 0.
namespace {
  double constexpr s_mu = 105.6583755; // MeV

  state curved_track() {
    state s;
    s.z        = 0.;
    s.par[QOP] = -1. / 1000.;
    for (unsigned int i = 0; i < s_dim; ++i) {
      s.cov(i, i) = 1e-4;
    }
    return s;
  }

  layer graphite_at(double z) {
    layer l;
    l.z                = z;
    l.thickness        = 4.;
    l.radiation_length = 191.5;
    l.density          = 2.23;
    l.z_over_a         = 0.49955;
    l.mean_excitation  = 78e-6;
    return l;
  }
} // namespace

BOOST_AUTO_TEST_CASE(curved_track_enters_and_leaves) {
  uniform_field const bf({0.6, 0., 0.});
  auto const in   = curved_track();
  auto const free = propagate(in, 1000., s_mu, bf);
  BOOST_REQUIRE(free);
  double const y_end = free->out.par[Y];
  BOOST_TEST_MESSAGE("y at z = 1000 mm: " << y_end << " mm");
  BOOST_REQUIRE(std::abs(y_end) > 60.);
  double const sgn = y_end > 0 ? 1. : -1.;

  // Enters: the extent starts 40 mm from the straight line, on the side the track bends to.
  layer enter  = graphite_at(1000.);
  enter.extent = transverse_extent{-500., 500., sgn > 0 ? 40. : -500., sgn > 0 ? 500. : -40.};
  BOOST_REQUIRE(!enter.extent->contains(0., 0.));
  BOOST_REQUIRE(enter.extent->contains(free->out.par[X], y_end));
  // Leaves: the extent is +-40 mm around the straight line.
  layer leave  = graphite_at(1000.);
  leave.extent = transverse_extent{-500., 500., -40., 40.};
  BOOST_REQUIRE(leave.extent->contains(0., 0.));
  BOOST_REQUIRE(!leave.extent->contains(free->out.par[X], y_end));

  auto const no_extent = graphite_at(1000.);
  auto const full      = propagate(in, 2000., s_mu, bf, fixed_material_budget({no_extent}));
  auto const entering  = propagate(in, 2000., s_mu, bf, fixed_material_budget({enter}));
  auto const leaving   = propagate(in, 2000., s_mu, bf, fixed_material_budget({leave}));
  auto const field     = propagate(in, 2000., s_mu, bf);
  BOOST_REQUIRE(full && entering && leaving && field);

  // Entering: the layer is applied, same result as a layer without extent
  BOOST_CHECK_LT(entering->out.momentum(), in.momentum());
  for (unsigned int i = 0; i < s_dim; ++i) {
    BOOST_CHECK_EQUAL(entering->out.par[i], full->out.par[i]);
    BOOST_CHECK_EQUAL(entering->out.cov(i, i), full->out.cov(i, i));
  }
  // Leaving: the layer is not applied, no energy loss and no scattering noise
  BOOST_CHECK_EQUAL(leaving->out.par[QOP], in.par[QOP]);
  BOOST_CHECK_CLOSE(leaving->out.par[Y], field->out.par[Y], 1e-6);
  BOOST_CHECK_CLOSE(leaving->out.cov(TX, TX), field->out.cov(TX, TX), 1e-6);
  BOOST_CHECK_GT(full->out.cov(TX, TX), field->out.cov(TX, TX) * 1.01);

  // Round trip through the entering layer (smoother direction): the energy is gained back on the same layer
  auto const back = propagate(entering->out, 0., s_mu, bf, fixed_material_budget({enter}));
  BOOST_REQUIRE(back);
  BOOST_CHECK_CLOSE(back->out.par[QOP], in.par[QOP], 1e-6);
  BOOST_CHECK_SMALL(back->out.par[Y] - in.par[Y], 1e-4);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
