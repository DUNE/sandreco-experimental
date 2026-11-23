#define BOOST_TEST_MODULE kalman_geometry_field
#include <boost/test/included/unit_test.hpp>

#include <kalman/engine.hpp>
#include <kalman/geometry_field.hpp>
#include <kalman/propagator.hpp>
#include <test_helpers.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/config.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>

// Geometry and GDML of the sandreco data in the image (the same files as the framework tests); the edep-sim file
// stores the TGeoManager without the GDML auxiliaries.
#ifndef SAND_IMAGE_DATA_DIR
#  error "SAND_IMAGE_DATA_DIR must be defined"
#endif

using namespace sand::tracker::kalman;

namespace {
  std::string const data_dir = SAND_IMAGE_DATA_DIR;
  std::string const gdml     = data_dir + "/geometries/sand/SAND_opt3_DRIFT1.gdml";
  std::string const edep     = data_dir + "/test/SAND_opt3_DRIFT1.sand-events-in-sand_inner_volume.2.edep.root";

  // Values of the BField tags of SAND_opt3_DRIFT1.gdml [T]
  double constexpr b_magnet = 0.6;
  double constexpr b_yoke   = -1.746067;

  // Centre of volSAND in the global frame [mm]
  double constexpr cy = -2384.73, cz = 23910.;

  /// The geometry is global (gGeoManager): one root_tgeomanager for the whole test program.
  sand::root_tgeomanager& geometry() {
    static sand::root_tgeomanager geo(ufw::json{{"geometry", edep}});
    return geo;
  }

  /// Field built once from the GDML tags, shared by the test cases.
  const volume_field& gdml_field() {
    static volume_field const f(geometry(), read_gdml_field_tags(gdml));
    return f;
  }

  /// Path of the deepest node at a point.
  std::string path_at(double x, double y, double z) {
    auto nav = geometry().navigator();
    nav->FindNode(x, y, z);
    return nav->GetPath();
  }

  bool contains(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

  void check_field(const field_vector& b, double bx, double by, double bz) {
    BOOST_CHECK_CLOSE_FRACTION(b[0] + 10., bx + 10., 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(b[1] + 10., by + 10., 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(b[2] + 10., bz + 10., 1e-12);
  }

  /// Write text to a new file in the temporary directory and return its path.
  std::filesystem::path write_temp(const std::string& name, const std::string& text) {
    auto const p = std::filesystem::temp_directory_path() / name;
    std::ofstream(p) << text;
    return p;
  }
} // namespace

BOOST_AUTO_TEST_SUITE(gdml_tags)

BOOST_AUTO_TEST_CASE(parse_values) {
  check_field(parse_bfield("(0.600000 T, 0.0 T, 0.0 T)"), 0.6, 0., 0.);
  check_field(parse_bfield("(0.0 T,  1.0 T, 0 T)"), 0., 1., 0.);
  check_field(parse_bfield("0.5 T, 0.0 T, 0.0 T"), 0.5, 0., 0.); // no parentheses, as in some GDML files
  check_field(parse_bfield(" ( -1.746067 tesla ,0,2e-1 ) "), -1.746067, 0., 0.2);
  check_field(parse_bfield("(10 kG, 2000 G, 3 kilogauss)"), 1., 0.2, 0.3);
  check_field(parse_bfield("(1, 2, 3)"), 1., 2., 3.); // no unit: tesla, as in edep-sim
  BOOST_CHECK_THROW(parse_bfield("(1 T, 2 T)"), std::exception);
  BOOST_CHECK_THROW(parse_bfield("(1 T, 2 T, 3 T, 4 T)"), std::exception);
  BOOST_CHECK_THROW(parse_bfield("(1 mT, 0 T, 0 T)"), std::exception);
  BOOST_CHECK_THROW(parse_bfield("(a T, 0 T, 0 T)"), std::exception);
  BOOST_CHECK_THROW(parse_bfield(""), std::exception);
}

BOOST_AUTO_TEST_CASE(read_small_file) {
  auto const p    = write_temp("kalman_geometry_field_small.gdml", R"xml(<?xml version="1.0"?>
<gdml>
  <structure>
    <volume name="plain"><materialref ref="Air"/></volume>
    <volume name="magnet">
      <auxiliary auxtype="SensDet" auxvalue="magnet"/>
      <auxiliary auxtype="BField" auxvalue="(0.6 T, 0 T, 0 T)"/>
    </volume>
    <!-- <volume name="commented"><auxiliary auxtype="BField" auxvalue="(9 T, 9 T, 9 T)"/></volume> -->
    <volume name="tpc"><auxiliary auxtype="EField" auxvalue="(500 V/cm, 0 V/cm, 0 V/cm)"/></volume>
    <volume name="both">
      <auxiliary auxtype="EField" auxvalue="(1 V/cm, 0 V/cm, 0 V/cm)"/>
      <auxiliary auxtype="BField" auxvalue="(0 T, 0 T, 0.2 T)"/>
    </volume>
  </structure>
</gdml>
)xml");
  auto const tags = read_gdml_field_tags(p);
  std::filesystem::remove(p);
  BOOST_REQUIRE_EQUAL(tags.size(), 3u);
  BOOST_CHECK_EQUAL(tags[0].volume, "magnet");
  check_field(tags[0].b, 0.6, 0., 0.);
  BOOST_CHECK_EQUAL(tags[1].volume, "tpc"); // EField only: B = 0
  check_field(tags[1].b, 0., 0., 0.);
  BOOST_CHECK_EQUAL(tags[2].volume, "both");
  check_field(tags[2].b, 0., 0., 0.2);
}

BOOST_AUTO_TEST_CASE(read_errors) {
  BOOST_CHECK_THROW(read_gdml_field_tags("/nonexistent/file.gdml"), std::exception);
  auto const p =
      write_temp("kalman_geometry_field_bad.gdml",
                 R"xml(<gdml><structure><volume name="v"><auxiliary auxtype="BField" auxvalue="(1 T, 2 T)"/>)xml"
                 R"xml(</volume></structure></gdml>)xml");
  BOOST_CHECK_THROW(read_gdml_field_tags(p), std::exception);
  std::filesystem::remove(p);
}

// The tags of the SAND GDML of the tests
BOOST_AUTO_TEST_CASE(read_sand_gdml) {
  auto const tags = read_gdml_field_tags(gdml);
  std::map<std::string, field_vector> by_name;
  for (auto const& t : tags) {
    by_name[t.volume] = t.b;
  }
  BOOST_REQUIRE(by_name.count("MagIntVol_volume"));
  BOOST_REQUIRE(by_name.count("KLOEYokeBarrel_volume"));
  BOOST_REQUIRE(by_name.count("volTPCActive"));
  check_field(by_name["MagIntVol_volume"], b_magnet, 0., 0.);
  check_field(by_name["KLOEYokeBarrel_volume"], b_yoke, 0., 0.);
  check_field(by_name["volTPCActive"], 0., 0., 0.);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(field_values)

// Inside the tracker: inherited from MagIntVol_volume through sand_inner_volume and the tracker volumes
BOOST_AUTO_TEST_CASE(tracker) {
  auto const& f = gdml_field();
  for (double x : {-1000., 0., 700.}) {
    for (double dy : {-800., 0., 500.}) {
      for (double dz : {-600., 0., 900.}) {
        auto const p = path_at(x, cy + dy, cz + dz);
        BOOST_TEST_CONTEXT(p) {
          BOOST_CHECK(contains(p, "sand_inner_volume"));
          check_field(f.at(x, cy + dy, cz + dz), b_magnet, 0., 0.);
        }
      }
    }
  }
  // a point deep in the tracker hierarchy
  auto const p = path_at(0., cy, cz);
  BOOST_CHECK(contains(p, "SANDtracker"));
  BOOST_CHECK(!contains(p.substr(p.rfind('/')), "MagIntVol"));
  check_field(f.at(0., cy, cz), b_magnet, 0., 0.);
}

// In the calorimeter, also a daughter of MagIntVol_volume
BOOST_AUTO_TEST_CASE(calorimeter) {
  auto const p = path_at(0., cy, cz + 2100.);
  BOOST_REQUIRE(contains(p, "kloe_calo_volume"));
  check_field(gdml_field().at(0., cy, cz + 2100.), b_magnet, 0., 0.);
}

// In the yoke barrel the field is its own tag; the endcaps have no tag
BOOST_AUTO_TEST_CASE(yoke) {
  auto const& f = gdml_field();
  BOOST_REQUIRE(contains(path_at(0., cy, cz + 3000.), "KLOEYokeBarrel_volume"));
  check_field(f.at(0., cy, cz + 3000.), b_yoke, 0., 0.);
  BOOST_REQUIRE(contains(path_at(0., cy + 3000., cz), "KLOEYokeBarrel_volume"));
  check_field(f.at(0., cy + 3000., cz), b_yoke, 0., 0.);
  // an endcap, at one end of the magnet axis (x); the endcaps are rings, so they are searched off axis
  bool found = false;
  for (double dy : {1500., 2000., 2500.}) {
    for (double x = 1000.; x < 5000. && !found; x += 5.) {
      if (contains(path_at(x, cy + dy, cz), "KLOEYokeEndcap")) {
        found = true;
        check_field(f.at(x, cy + dy, cz), 0., 0., 0.);
      }
    }
  }
  BOOST_CHECK(found);
}

// Outside the detector, and outside the world
BOOST_AUTO_TEST_CASE(outside) {
  auto const& f = gdml_field();
  BOOST_CHECK(!contains(path_at(0., cy, cz + 6000.), "volSAND_PV"));
  check_field(f.at(0., cy, cz + 6000.), 0., 0., 0.);
  check_field(f.at(0., 0., 1e9), 0., 0., 0.);
  // back inside after leaving the world
  check_field(f.at(0., cy, cz), b_magnet, 0., 0.);
}

// The cached volume must not be trusted when the point moves or the shared navigator is moved by other code.
BOOST_AUTO_TEST_CASE(cache) {
  auto const& f = gdml_field();
  auto other    = geometry().navigator();
  for (int i = 0; i < 20; ++i) {
    check_field(f.at(0., cy, cz), b_magnet, 0., 0.);
    check_field(f.at(0., cy, cz + 3000.), b_yoke, 0., 0.);
    other->FindNode(0., cy, cz + 3000.); // someone else moves the navigator into the yoke
    check_field(f.at(0., cy, cz + 1.), b_magnet, 0., 0.);
    other->FindNode(0., cy, cz); // ... or into the tracker
    check_field(f.at(0., cy, cz + 3000.), b_yoke, 0., 0.);
    check_field(f.at(0., cy, cz + 6000.), 0., 0., 0.);
  }
}

// In the tracker the field is uniform: the propagation is the same as with the uniform field.
BOOST_AUTO_TEST_CASE(propagation_in_tracker) {
  uniform_field const uniform({b_magnet, 0., 0.});
  state s;
  s.z        = cz - 1000.;
  s.par[X]   = 100.;
  s.par[Y]   = cy + 200.;
  s.par[TX]  = 0.1;
  s.par[TY]  = -0.2;
  s.par[QOP] = -1. / 800.;
  for (unsigned int i = 0; i < s_dim; ++i) {
    s.cov(i, i) = 1.;
  }
  auto const a = propagate(s, cz + 1000., 105.6583755, gdml_field());
  auto const b = propagate(s, cz + 1000., 105.6583755, uniform);
  BOOST_REQUIRE(a && b);
  for (unsigned int i = 0; i < s_dim; ++i) {
    BOOST_CHECK_EQUAL(a->out.par[i], b->out.par[i]);
    for (unsigned int j = 0; j < s_dim; ++j) {
      BOOST_CHECK_EQUAL(a->F(i, j), b->F(i, j));
    }
  }
}

// Time per lookup on straight lines through SAND; printed, not checked (it depends on the machine and the build).
BOOST_AUTO_TEST_CASE(timing) {
  auto const& f = gdml_field();
  for (double step : {0.5, 5., 25.}) {
    long n     = 0;
    double sum = 0.;
    auto t0    = std::chrono::steady_clock::now();
    for (int line = 0; line < 50; ++line) {
      double const x = -1500. + 60. * line, y = cy - 1500. + 60. * line, tx = 0.3 - 0.012 * line, ty = -0.2;
      for (double z = cz - 2500.; z < cz + 2500.; z += step) {
        sum += f.at(x + tx * (z - cz), y + ty * (z - cz), z)[0];
        ++n;
      }
    }
    double const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / n;
    BOOST_TEST_MESSAGE("volume_field::at, points " << step << " mm apart: " << ns << " ns per call (" << n
                                                   << " calls, sum " << sum << ")");
    BOOST_CHECK(std::isfinite(sum));
  }
}

// Summary of the table: the volumes counted under tags with a non-zero field are those with a non-zero field.
BOOST_AUTO_TEST_CASE(summary) {
  auto const& f = gdml_field();
  bool magnet = false, yoke = false;
  for (auto const& u : f.tag_usage()) {
    if (u.volume == "MagIntVol_volume") {
      magnet = u.placed && u.inheriting > 0 && u.geometry_volume == "MagIntVol_volume_PV";
    }
    if (u.volume == "KLOEYokeBarrel_volume") {
      yoke = u.placed && u.b[0] == b_yoke;
    }
  }
  BOOST_CHECK(magnet);
  BOOST_CHECK(yoke);
  BOOST_CHECK_GT(f.untagged_volumes(), 0u);
  std::size_t nonzero = 0;
  for (auto const& u : f.tag_usage()) {
    if (u.b != field_vector{}) {
      nonzero += u.inheriting + (u.placed ? 1 : 0);
    }
  }
  BOOST_CHECK_EQUAL(nonzero, f.magnetised_volumes());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inheritance)

// A volume tagged with EField only hides the field of its ancestors, as in edep-sim.
BOOST_AUTO_TEST_CASE(efield_only_hides_ancestor) {
  volume_field const f(geometry(), {{"MagIntVol_volume", {b_magnet, 0., 0.}}, {"sand_inner_volume", {0., 0., 0.}}});
  check_field(f.at(0., cy, cz), 0., 0., 0.);               // tracker, under sand_inner_volume
  check_field(f.at(0., cy, cz + 2100.), b_magnet, 0., 0.); // calorimeter
  check_field(f.at(0., cy, cz + 3000.), 0., 0., 0.);       // yoke: not tagged here
  BOOST_CHECK_EQUAL(f.volume_value(nullptr)[0], 0.);       // unknown volume
  BOOST_CHECK_GT(f.magnetised_volumes(), 0u);
}

// A daughter with its own tag overrides its mother's field.
BOOST_AUTO_TEST_CASE(daughter_overrides) {
  volume_field const f(geometry(), {{"MagIntVol_volume", {b_magnet, 0., 0.}}, {"SANDtracker", {0., 0., 0.3}}});
  check_field(f.at(0., cy, cz), 0., 0., 0.3);
  check_field(f.at(0., cy, cz + 2100.), b_magnet, 0., 0.);
}

BOOST_AUTO_TEST_CASE(missing_volume) {
  BOOST_CHECK_THROW(volume_field(geometry(), {{"no_such_volume", {1., 0., 0.}}}), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(engine_configuration)

// Configuration errors of the "gdml" form are found before the geometry is needed.
BOOST_AUTO_TEST_CASE(invalid) {
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "file" : "x.gdml" } })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : "geometry" } })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : 1, "file" : "x.gdml" } })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : "gdml" } })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : "gdml", "file" : 3 } })")), std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : "gdml", "file" : "/nonexistent.gdml" } })")),
                    std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : { "source" : "gdml", "file" : "x", "files" : 1 } })")),
                    std::exception);
  BOOST_CHECK_THROW(engine(ufw::json::parse(R"({ "field" : [0.6, "0", 0] })")), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
