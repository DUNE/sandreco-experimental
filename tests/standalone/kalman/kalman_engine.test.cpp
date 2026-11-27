#define BOOST_TEST_MODULE kalman_engine
#include <boost/test/included/unit_test.hpp>

#include <kalman/engine.hpp>
#include <kalman/measurement.hpp>
#include <kalman/particle.hpp>
#include <kalman/propagator.hpp>
#include <test_helpers.hpp>

#include <ufw/config.hpp>

#include <cmath>
#include <exception>
#include <string>

using namespace sand::tracker::kalman;

namespace {
  particle_hypothesis const mu = hypothesis_from_pdg(13); // muon: mass from the table, |q| = 1

  state make_state() {
    state s;
    s.z        = 0.;
    s.par[X]   = 10.;
    s.par[Y]   = -20.;
    s.par[TX]  = 0.3;
    s.par[TY]  = -0.2;
    s.par[QOP] = -1. / 700.;
    s.par[T]   = 5.;
    for (unsigned int i = 0; i < s_dim; ++i) {
      s.cov(i, i) = 1e-2 * (i + 1);
    }
    s.cov(X, TX) = 1e-4;
    return s;
  }

  ufw::config full_config() {
    return ufw::json::parse(R"({
      "field" : [0.6, 0.05, -0.02],
      "calibration" : { "drift_velocity" : 0.04, "wire_velocity" : 180.0, "time_resolution" : 2.5, "t0_offset" : 12.0 },
      "propagator" : { "max_step" : 20.0, "max_slope" : 50.0 }
    })");
  }
} // namespace

BOOST_AUTO_TEST_SUITE(engine_tests)

// Every calibration and propagator key is required: removing any one of them, or a whole section, is an error that
// names the missing key.
BOOST_AUTO_TEST_CASE(missing_keys) {
  struct missing {
    const char* section;
    const char* key; // nullptr: the whole section is removed
    const char* name;
  };
  missing const cases[] = {
      {"calibration", "drift_velocity", "calibration.drift_velocity"},
      {"calibration", "wire_velocity", "calibration.wire_velocity"},
      {"calibration", "time_resolution", "calibration.time_resolution"},
      {"calibration", "t0_offset", "calibration.t0_offset"},
      {"propagator", "max_step", "propagator.max_step"},
      {"propagator", "max_slope", "propagator.max_slope"},
      {"calibration", nullptr, "calibration"},
      {"propagator", nullptr, "propagator"},
  };
  // The full configuration is valid
  BOOST_CHECK_NO_THROW(engine{full_config()});
  for (auto const& c : cases) {
    ufw::config cfg = full_config();
    if (c.key) {
      cfg.at(c.section).erase(c.key);
    } else {
      cfg.erase(c.section);
    }
    bool thrown = false;
    try {
      engine const kf(cfg);
    } catch (const std::exception& e) {
      thrown = true;
      BOOST_CHECK_MESSAGE(std::string(e.what()).find("'" + std::string(c.name) + "'") != std::string::npos,
                          "the error for a missing '" << c.name << "' does not name it: " << e.what());
    }
    BOOST_CHECK_MESSAGE(thrown, "no error for a missing '" << c.name << "'");
  }
}

BOOST_AUTO_TEST_CASE(configured_values) {
  engine const kf(full_config());
  BOOST_CHECK_EQUAL(kf.calibration().drift_velocity, 0.04);
  BOOST_CHECK_EQUAL(kf.calibration().wire_velocity, 180.);
  BOOST_CHECK_EQUAL(kf.calibration().time_resolution, 2.5);
  BOOST_CHECK_EQUAL(kf.calibration().t0_offset, 12.);
  BOOST_CHECK_EQUAL(kf.propagator_settings().max_step, 20.);
  BOOST_CHECK_EQUAL(kf.propagator_settings().max_slope, 50.);
  auto const b = kf.field().at(-500., 300., 1e4);
  BOOST_CHECK_EQUAL(b[0], 0.6);
  BOOST_CHECK_EQUAL(b[1], 0.05);
  BOOST_CHECK_EQUAL(b[2], -0.02);
}

// engine.propagate must be the free propagate with the same field and settings: same code, so identical results.
BOOST_AUTO_TEST_CASE(propagate_matches_free_function) {
  engine const kf(full_config());
  uniform_field const bf({0.6, 0.05, -0.02});
  propagator_config const pc{20., 50.};
  auto const in = make_state();
  for (double z : {1500., -800.}) {
    auto const a = kf.propagate(in, z, mu);
    auto const b = propagate(in, z, mu, bf, pc);
    BOOST_REQUIRE(a && b);
    BOOST_CHECK_EQUAL(a->out.z, b->out.z);
    BOOST_CHECK_EQUAL(a->out.direction, b->out.direction);
    BOOST_CHECK_EQUAL(a->path_length, b->path_length);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(a->out.par[i], b->out.par[i]);
      for (unsigned int j = 0; j < s_dim; ++j) {
        BOOST_CHECK_EQUAL(a->F(i, j), b->F(i, j));
        BOOST_CHECK_EQUAL(a->out.cov(i, j), b->out.cov(i, j));
      }
    }
  }
  // The settings of the engine are used: with a coarser step the result differs (Runge-Kutta truncation).
  auto const coarse = propagate(in, 1500., mu, bf, {200., 50.});
  auto const fine   = kf.propagate(in, 1500., mu);
  BOOST_REQUIRE(coarse && fine);
  BOOST_CHECK_NE(coarse->out.par[Y], fine->out.par[Y]);
  // and max_slope too: a straight track (q/p = 0) with ty = 60 keeps its slope, so it propagates with a limit of
  // 1000 but not with the one of the engine (50).
  state steep    = make_state();
  steep.par[TY]  = 60.;
  steep.par[QOP] = 0.;
  BOOST_CHECK(!kf.propagate(steep, 10., mu));
  BOOST_CHECK(propagate(steep, 10., mu, bf, {20., 1e3}));
}

// engine.wire_time must be the free wire_time with the calibration of the engine.
BOOST_AUTO_TEST_CASE(wire_time_matches_free_function) {
  engine const kf(full_config());
  auto s = make_state();
  s.z    = 500.;
  wire_geometry const w{{-2000., -15., 500.}, {2000., -15., 500.}};
  for (int side : {+1, -1}) {
    auto const a = kf.wire_time(s, w, side);
    auto const b = wire_time(s, w, side, kf.calibration());
    BOOST_CHECK_EQUAL(a.value, b.value);
    for (unsigned int i = 0; i < s_dim; ++i) {
      BOOST_CHECK_EQUAL(a.H[i], b.H[i]);
    }
  }
  // the configured offset enters the prediction
  reco_calibration no_offset = kf.calibration();
  no_offset.t0_offset        = 0.;
  BOOST_CHECK_CLOSE(kf.wire_time(s, w, +1).value - wire_time(s, w, +1, no_offset).value, 12., 1e-9);
}

BOOST_AUTO_TEST_CASE(invalid_configurations) {
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({})")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, 0.0] })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : 0.6 })")), std::exception);
  // Only the field: the calibration and propagator sections are required
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, 0.0, 0.0] })")), std::exception);
  // Each of the following is the full configuration with one change
  auto with = [](const char* section, const char* key, const ufw::json& value) {
    ufw::config cfg = full_config();
    if (key) {
      cfg.at(section)[key] = value;
    } else {
      cfg[section] = value;
    }
    return cfg;
  };
  BOOST_CHECK_THROW(engine(with("fields", nullptr, 1)), std::exception);
  BOOST_CHECK_THROW(engine(with("calibration", "drift_vel", 1)), std::exception);
  // The old name of the time resolution is an unknown key, so a configuration that still uses it fails
  BOOST_CHECK_THROW(engine(with("calibration", "tdc_resolution", 3.5)), std::exception);
  BOOST_CHECK_THROW(engine(with("calibration", "drift_velocity", 0)), std::exception);
  BOOST_CHECK_THROW(engine(with("calibration", "wire_velocity", "fast")), std::exception);
  BOOST_CHECK_THROW(engine(with("calibration", "t0_offset", nullptr)), std::exception);
  BOOST_CHECK_THROW(engine(with("propagator", "max_step", -1)), std::exception);
  BOOST_CHECK_THROW(engine(with("propagator", nullptr, 3)), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
