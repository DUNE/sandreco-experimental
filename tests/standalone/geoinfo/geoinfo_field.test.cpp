#define BOOST_TEST_MODULE geoinfo_field
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>

#include <geoinfo/field_info.hpp>
#include <geoinfo/geoinfo.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>

// Geometry and GDML of the sandreco data in the image (the same files as the framework tests); the edep-sim file
// stores the TGeoManager without the GDML auxiliaries.
#ifndef SAND_IMAGE_DATA_DIR
#  error "SAND_IMAGE_DATA_DIR must be defined"
#endif

using sand::dir_3d;
using sand::geoinfo;
using sand::pos_3d;

namespace {
  std::string const data_dir = SAND_IMAGE_DATA_DIR;
  std::string const gdml_rel = "geometries/sand/SAND_opt3_DRIFT1.gdml";
  std::string const gdml     = data_dir + "/" + gdml_rel;
  std::string const edep     = data_dir + "/test/SAND_opt3_DRIFT1.sand-events-in-sand_inner_volume.2.edep.root";

  // Values of the BField tags of SAND_opt3_DRIFT1.gdml [T]
  double constexpr b_magnet = 0.6;
  double constexpr b_yoke   = -1.746067;

  // Centre of volSAND in the global frame [mm]
  double constexpr cy = -2384.73, cz = 23910.;

  /// The job context, with the geometry as a global: one root_tgeomanager for the whole test program.
  ufw::context& context() {
    static ufw::context ctx;
    static bool const initialized = [] {
      ufw::config::set_basepath(data_dir);
      ctx.init(ufw::json{{"globals", {{"sand::root_tgeomanager", {{"geometry", edep}}}}}, {"contexts", {{"keys", 1}}}});
      ctx.before_run();
      return true;
    }();
    (void)initialized;
    return ctx;
  }

  sand::root_tgeomanager& geometry() { return context().instance<sand::root_tgeomanager>(); }

  /// geoinfo with the given "field" configuration (an absent field if cfg is null).
  geoinfo make_geoinfo(const ufw::json& field) {
    context();
    ufw::json cfg = ufw::json::object();
    if (!field.is_null()) {
      cfg["field"] = field;
    }
    return geoinfo(ufw::config(cfg));
  }

  /// geoinfo with the field of the GDML, built once and shared by the test cases.
  const geoinfo& gdml_geoinfo() {
    static geoinfo const g = make_geoinfo({{"source", "gdml"}, {"file", gdml}});
    return g;
  }

  const geoinfo::volume_field& gdml_field() {
    return dynamic_cast<const geoinfo::volume_field&>(gdml_geoinfo().field());
  }

  /// Path of the deepest node at a point.
  std::string path_at(double x, double y, double z) {
    auto nav = geometry().navigator();
    nav->FindNode(x, y, z);
    return nav->GetPath();
  }

  bool contains(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

  void check_field(const dir_3d& b, double bx, double by, double bz) {
    BOOST_CHECK_CLOSE_FRACTION(b.X() + 10., bx + 10., 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(b.Y() + 10., by + 10., 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(b.Z() + 10., bz + 10., 1e-12);
  }

  /// Field at (x, y, z) from geoinfo::field_at and from a new probe, checked to be equal.
  dir_3d both(const geoinfo& g, double x, double y, double z) {
    auto probe   = g.make_field_probe();
    auto const a = g.field_at(pos_3d(x, y, z));
    auto const b = probe.field_at(pos_3d(x, y, z));
    BOOST_CHECK(a == b);
    return a;
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
  check_field(sand::parse_bfield("(0.600000 T, 0.0 T, 0.0 T)"), 0.6, 0., 0.);
  check_field(sand::parse_bfield("(0.0 T,  1.0 T, 0 T)"), 0., 1., 0.);
  check_field(sand::parse_bfield("0.5 T, 0.0 T, 0.0 T"), 0.5, 0., 0.); // no parentheses, as in some GDML files
  check_field(sand::parse_bfield(" ( -1.746067 tesla ,0,2e-1 ) "), -1.746067, 0., 0.2);
  check_field(sand::parse_bfield("(10 kG, 2000 G, 3 kilogauss)"), 1., 0.2, 0.3);
  check_field(sand::parse_bfield("(1, 2, 3)"), 1., 2., 3.); // no unit: tesla, as in edep-sim
  BOOST_CHECK_THROW(sand::parse_bfield("(1 T, 2 T)"), std::exception);
  BOOST_CHECK_THROW(sand::parse_bfield("(1 T, 2 T, 3 T, 4 T)"), std::exception);
  BOOST_CHECK_THROW(sand::parse_bfield("(1 mT, 0 T, 0 T)"), std::exception);
  BOOST_CHECK_THROW(sand::parse_bfield("(a T, 0 T, 0 T)"), std::exception);
  BOOST_CHECK_THROW(sand::parse_bfield(""), std::exception);
}

BOOST_AUTO_TEST_CASE(read_small_file) {
  auto const p    = write_temp("geoinfo_field_small.gdml", R"xml(<?xml version="1.0"?>
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
  auto const tags = sand::read_gdml_field_tags(p);
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
  BOOST_CHECK_THROW(sand::read_gdml_field_tags("/nonexistent/file.gdml"), std::exception);
  auto const p =
      write_temp("geoinfo_field_bad.gdml",
                 R"xml(<gdml><structure><volume name="v"><auxiliary auxtype="BField" auxvalue="(1 T, 2 T)"/>)xml"
                 R"xml(</volume></structure></gdml>)xml");
  BOOST_CHECK_THROW(sand::read_gdml_field_tags(p), std::exception);
  std::filesystem::remove(p);
}

// The tags of the SAND GDML of the tests
BOOST_AUTO_TEST_CASE(read_sand_gdml) {
  auto const tags = sand::read_gdml_field_tags(gdml);
  std::map<std::string, dir_3d> by_name;
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
  auto const& g = gdml_geoinfo();
  for (double x : {-1000., 0., 700.}) {
    for (double dy : {-800., 0., 500.}) {
      for (double dz : {-600., 0., 900.}) {
        auto const p = path_at(x, cy + dy, cz + dz);
        BOOST_TEST_CONTEXT(p) {
          BOOST_CHECK(contains(p, "sand_inner_volume"));
          check_field(both(g, x, cy + dy, cz + dz), b_magnet, 0., 0.);
        }
      }
    }
  }
  // a point deep in the tracker hierarchy
  auto const p = path_at(0., cy, cz);
  BOOST_CHECK(contains(p, "SANDtracker"));
  BOOST_CHECK(!contains(p.substr(p.rfind('/')), "MagIntVol"));
  check_field(both(g, 0., cy, cz), b_magnet, 0., 0.);
}

// In the calorimeter, also a daughter of MagIntVol_volume
BOOST_AUTO_TEST_CASE(calorimeter) {
  auto const p = path_at(0., cy, cz + 2100.);
  BOOST_REQUIRE(contains(p, "kloe_calo_volume"));
  check_field(both(gdml_geoinfo(), 0., cy, cz + 2100.), b_magnet, 0., 0.);
}

// In the yoke barrel the field is its own tag; the endcaps have no tag
BOOST_AUTO_TEST_CASE(yoke) {
  auto const& g = gdml_geoinfo();
  BOOST_REQUIRE(contains(path_at(0., cy, cz + 3000.), "KLOEYokeBarrel_volume"));
  check_field(both(g, 0., cy, cz + 3000.), b_yoke, 0., 0.);
  BOOST_REQUIRE(contains(path_at(0., cy + 3000., cz), "KLOEYokeBarrel_volume"));
  check_field(both(g, 0., cy + 3000., cz), b_yoke, 0., 0.);
  // an endcap, at one end of the magnet axis (x); the endcaps are rings, so they are searched off axis
  bool found = false;
  for (double dy : {1500., 2000., 2500.}) {
    for (double x = 1000.; x < 5000. && !found; x += 5.) {
      if (contains(path_at(x, cy + dy, cz), "KLOEYokeEndcap")) {
        found = true;
        check_field(both(g, x, cy + dy, cz), 0., 0., 0.);
      }
    }
  }
  BOOST_CHECK(found);
}

// Outside the detector, and outside the world
BOOST_AUTO_TEST_CASE(outside) {
  auto const& g = gdml_geoinfo();
  BOOST_CHECK(!contains(path_at(0., cy, cz + 6000.), "volSAND_PV"));
  check_field(both(g, 0., cy, cz + 6000.), 0., 0., 0.);
  check_field(both(g, 0., 0., 1e9), 0., 0., 0.);
  // back inside after leaving the world, with the same probe
  auto probe = g.make_field_probe();
  check_field(probe.field_at(pos_3d(0., 0., 1e9)), 0., 0., 0.);
  check_field(probe.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);
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
      yoke = u.placed && u.b.X() == b_yoke;
    }
  }
  BOOST_CHECK(magnet);
  BOOST_CHECK(yoke);
  BOOST_CHECK_GT(f.untagged_volumes(), 0u);
  std::size_t nonzero = 0;
  for (auto const& u : f.tag_usage()) {
    if (u.b != dir_3d()) {
      nonzero += u.inheriting + (u.placed ? 1 : 0);
    }
  }
  BOOST_CHECK_EQUAL(nonzero, f.magnetised_volumes());
  BOOST_CHECK(contains(f.description(), gdml.c_str()));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(probe)

// A probe and field_at agree on random points in and around SAND, visited in random order and along lines.
BOOST_AUTO_TEST_CASE(same_as_field_at) {
  auto const& g = gdml_geoinfo();
  auto probe    = g.make_field_probe();
  std::mt19937_64 rng(12345);
  std::uniform_real_distribution<double> ux(-4000., 4000.), uy(-4000., 4000.), uz(-4000., 4000.);
  int different = 0, magnet = 0, yoke = 0, zero = 0;
  for (int i = 0; i < 100000; ++i) {
    pos_3d const p(ux(rng), cy + uy(rng), cz + uz(rng));
    auto const a = g.field_at(p);
    auto const b = probe.field_at(p);
    different += a != b;
    magnet += a.X() == b_magnet;
    yoke += a.X() == b_yoke;
    zero += a == dir_3d();
  }
  for (int line = 0; line < 20; ++line) {
    double const x = -1500. + 150. * line, y = cy - 1500. + 150. * line, tx = 0.3 - 0.03 * line, ty = -0.2;
    for (double z = cz - 4000.; z < cz + 4000.; z += 1.) {
      pos_3d const p(x + tx * (z - cz), y + ty * (z - cz), z);
      different += g.field_at(p) != probe.field_at(p);
    }
  }
  BOOST_CHECK_EQUAL(different, 0);
  // the sample covers the three values
  BOOST_CHECK_GT(magnet, 1000);
  BOOST_CHECK_GT(yoke, 1000);
  BOOST_CHECK_GT(zero, 1000);
}

// Other code moving the shared navigator does not affect the probe, and the probe does not move it.
BOOST_AUTO_TEST_CASE(independent_of_shared_navigator) {
  auto const& g = gdml_geoinfo();
  auto probe    = g.make_field_probe();
  auto other    = geometry().navigator();
  for (int i = 0; i < 20; ++i) {
    check_field(probe.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);
    check_field(probe.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
    other->FindNode(0., cy, cz + 3000.); // someone else moves the navigator into the yoke
    check_field(probe.field_at(pos_3d(0., cy, cz + 1.)), b_magnet, 0., 0.);
    other->FindNode(0., cy, cz); // ... or into the tracker
    check_field(probe.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
    check_field(probe.field_at(pos_3d(0., cy, cz + 6000.)), 0., 0., 0.);
    other->cd("/volWorld_PV_1"); // ... or to a path
    check_field(probe.field_at(pos_3d(0., cy, cz + 2100.)), b_magnet, 0., 0.);
  }

  // probe lookups leave the shared navigator where it was, and do not change the current navigator
  TGeoNavigator const* const current = gGeoManager->GetCurrentNavigator();
  other->FindNode(0., cy, cz + 3000.);
  std::string const path = other->GetPath();
  auto probe2            = g.make_field_probe();
  for (double z = cz - 3000.; z < cz + 3000.; z += 10.) {
    probe2.field_at(pos_3d(0., cy, z));
  }
  BOOST_CHECK_EQUAL(other->GetPath(), path);
  BOOST_CHECK_EQUAL(other->get_point().Z(), cz + 3000.);
  BOOST_CHECK(gGeoManager->GetCurrentNavigator() == current);
  BOOST_CHECK_EQUAL(gGeoManager->GetListOfNavigators()->GetEntriesFast(), 1);
}

// field_at of geoinfo stays correct whatever the shared navigator did before.
BOOST_AUTO_TEST_CASE(field_at_with_moved_navigator) {
  auto const& g = gdml_geoinfo();
  auto other    = geometry().navigator();
  other->FindNode(0., cy, cz + 3000.);
  check_field(g.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);
  other->FindNode(0., cy, cz);
  check_field(g.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
}

// Probes are independent of each other, movable and usable after a move; an unbound probe throws.
BOOST_AUTO_TEST_CASE(move_and_unbound) {
  auto const& g = gdml_geoinfo();
  geoinfo::field_probe unbound;
  BOOST_CHECK(!unbound);
  BOOST_CHECK_THROW(unbound.field_at(pos_3d(0., cy, cz)), std::exception);

  auto a = g.make_field_probe();
  auto b = g.make_field_probe();
  check_field(a.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);
  check_field(b.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
  check_field(a.field_at(pos_3d(0., cy, cz + 1.)), b_magnet, 0., 0.);

  geoinfo::field_probe kept;
  kept = std::move(a);
  BOOST_CHECK(kept);
  check_field(kept.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
  geoinfo::field_probe moved(std::move(kept));
  check_field(moved.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);

  // moved-from probes are unbound and throw instead of using a missing navigator
  BOOST_CHECK(!a);
  BOOST_CHECK(!kept);
  BOOST_CHECK_THROW(a.field_at(pos_3d(0., cy, cz)), std::exception);
  BOOST_CHECK_THROW(kept.field_at(pos_3d(0., cy, cz)), std::exception);
  // a moved-from probe can be bound again
  a = g.make_field_probe();
  check_field(a.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
}

// Time per lookup on straight lines through SAND, with and without a probe; printed, not checked (it depends on the
// machine and the build).
BOOST_AUTO_TEST_CASE(timing) {
  auto const& g = gdml_geoinfo();
  auto probe    = g.make_field_probe();
  for (bool with_probe : {true, false}) {
    for (double step : {0.5, 5., 25.}) {
      long n     = 0;
      double sum = 0.;
      auto t0    = std::chrono::steady_clock::now();
      for (int line = 0; line < 50; ++line) {
        double const x = -1500. + 60. * line, y = cy - 1500. + 60. * line, tx = 0.3 - 0.012 * line, ty = -0.2;
        for (double z = cz - 2500.; z < cz + 2500.; z += step) {
          pos_3d const p(x + tx * (z - cz), y + ty * (z - cz), z);
          sum += with_probe ? probe.field_at(p).X() : g.field_at(p).X();
          ++n;
        }
      }
      double const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / n;
      BOOST_TEST_MESSAGE((with_probe ? "field_probe::field_at" : "geoinfo::field_at")
                         << ", points " << step << " mm apart: " << ns << " ns per call (" << n << " calls, sum " << sum
                         << ")");
      BOOST_CHECK(std::isfinite(sum));
    }
  }
  int constexpr probes = 1000;
  auto t0              = std::chrono::steady_clock::now();
  for (int i = 0; i < probes; ++i) {
    auto p = g.make_field_probe();
    (void)p;
  }
  double const us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / probes;
  BOOST_TEST_MESSAGE("geoinfo::make_field_probe: " << us << " us per probe");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(configuration)

// A uniform field: the same value everywhere, also outside the world, from field_at and from a probe.
BOOST_AUTO_TEST_CASE(uniform) {
  auto const g = make_geoinfo(ufw::json::array({0.1, -0.2, 0.3}));
  BOOST_REQUIRE(g.has_field());
  BOOST_REQUIRE(dynamic_cast<const geoinfo::uniform_field*>(&g.field()));
  for (auto const& p : {pos_3d(0., cy, cz), pos_3d(0., cy, cz + 3000.), pos_3d(0., 0., 1e9)}) {
    check_field(g.field_at(p), 0.1, -0.2, 0.3);
    auto probe = g.make_field_probe();
    check_field(probe.field_at(p), 0.1, -0.2, 0.3);
  }
  auto const zero = make_geoinfo(ufw::json::array({0, 0, 0})); // integers are numbers too
  check_field(zero.field_at(pos_3d(0., cy, cz)), 0., 0., 0.);
}

// Without a "field" key: no field, a clear error from field_at and make_field_probe, nothing else changes.
BOOST_AUTO_TEST_CASE(missing_key) {
  auto const g = make_geoinfo(ufw::json());
  BOOST_CHECK(!g.has_field());
  BOOST_CHECK_THROW(g.field(), geoinfo::field_not_configured);
  BOOST_CHECK_THROW(g.field_at(pos_3d(0., cy, cz)), geoinfo::field_not_configured);
  BOOST_CHECK_THROW(g.make_field_probe(), geoinfo::field_not_configured);
  try {
    g.field_at(pos_3d(0., cy, cz));
  } catch (const std::exception& e) {
    BOOST_CHECK(contains(e.what(), "no magnetic field configured"));
    BOOST_CHECK(contains(e.what(), "\"field\""));
  }
  // other geoinfo services still work
  BOOST_CHECK_NO_THROW(g.tracker());
}

// A relative GDML path is resolved against ufw-basepath.
BOOST_AUTO_TEST_CASE(relative_path) {
  auto const g = make_geoinfo({{"source", "gdml"}, {"file", gdml_rel}});
  check_field(g.field_at(pos_3d(0., cy, cz)), b_magnet, 0., 0.);
  check_field(g.field_at(pos_3d(0., cy, cz + 3000.)), b_yoke, 0., 0.);
}

// Configuration errors are reported with the offending value, before any file is read.
BOOST_AUTO_TEST_CASE(invalid) {
  auto const bad = [](const char* text) {
    BOOST_TEST_CONTEXT(text) { BOOST_CHECK_THROW(make_geoinfo(ufw::json::parse(text)), std::exception); }
  };
  bad(R"({ "file" : "x.gdml" })");
  bad(R"({ "source" : "geometry" })");
  bad(R"({ "source" : 1, "file" : "x.gdml" })");
  bad(R"({ "source" : "gdml" })");
  bad(R"({ "source" : "gdml", "file" : 3 })");
  bad(R"({ "source" : "gdml", "file" : "/nonexistent.gdml" })");
  bad(R"({ "source" : "gdml", "file" : "x", "files" : 1 })");
  bad(R"([0.6, "0", 0])");
  bad(R"([0.6, 0])");
  bad(R"([0.6, 0, 0, 0])");
  bad(R"(0.6)");
  bad(R"("gdml")");
  try {
    make_geoinfo(ufw::json::parse(R"({ "source" : "gdml", "file" : "x", "files" : 1 })"));
  } catch (const std::exception& e) {
    BOOST_CHECK(contains(e.what(), "'files'"));
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inheritance)

// A volume tagged with EField only hides the field of its ancestors, as in edep-sim.
BOOST_AUTO_TEST_CASE(efield_only_hides_ancestor) {
  geoinfo::volume_field const f(geometry(),
                                {{"MagIntVol_volume", {b_magnet, 0., 0.}}, {"sand_inner_volume", {0., 0., 0.}}});
  check_field(f.at(pos_3d(0., cy, cz)), 0., 0., 0.);               // tracker, under sand_inner_volume
  check_field(f.at(pos_3d(0., cy, cz + 2100.)), b_magnet, 0., 0.); // calorimeter
  check_field(f.at(pos_3d(0., cy, cz + 3000.)), 0., 0., 0.);       // yoke: not tagged here
  BOOST_CHECK_EQUAL(f.volume_value(nullptr).X(), 0.);              // unknown volume
  BOOST_CHECK_GT(f.magnetised_volumes(), 0u);
  geoinfo::field_probe probe(f);
  check_field(probe.field_at(pos_3d(0., cy, cz)), 0., 0., 0.);
  check_field(probe.field_at(pos_3d(0., cy, cz + 2100.)), b_magnet, 0., 0.);
}

// A daughter with its own tag overrides its mother's field.
BOOST_AUTO_TEST_CASE(daughter_overrides) {
  geoinfo::volume_field const f(geometry(), {{"MagIntVol_volume", {b_magnet, 0., 0.}}, {"SANDtracker", {0., 0., 0.3}}});
  check_field(f.at(pos_3d(0., cy, cz)), 0., 0., 0.3);
  check_field(f.at(pos_3d(0., cy, cz + 2100.)), b_magnet, 0., 0.);
}

BOOST_AUTO_TEST_CASE(missing_volume) {
  BOOST_CHECK_THROW(geoinfo::volume_field(geometry(), {{"no_such_volume", {1., 0., 0.}}}), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

FIX_TEST_EXIT
