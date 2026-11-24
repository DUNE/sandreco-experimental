#define BOOST_TEST_MODULE kalman_geometry_field_synthetic
#include <boost/test/included/unit_test.hpp>

#include <kalman/geometry_field.hpp>
#include <test_helpers.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/config.hpp>

#include <TGeoManager.h>
#include <TGeoMaterial.h>
#include <TGeoMatrix.h>
#include <TGeoMedium.h>
#include <TGeoVolume.h>

#include <exception>
#include <filesystem>
#include <string>

// Inheritance rules of volume_field on a small geometry built here:
//
//   world_PV (box, half size 1000 mm)
//   |- A    at x = -300 mm (box, half size 100 mm)   named as in the GDML
//   |  `- C at its centre (box, half size 10 mm)
//   `- B_PV at x = +300 mm (box, half size 100 mm)   named as written by edep-sim
//      `- C at its centre: the same logical volume as in A
// and a volume E, defined but not placed.
//
// The geometry is global (one TGeoManager per program), hence a test program of its own.

using namespace sand::tracker::kalman;

namespace {

  /// Build the geometry, write it to a ROOT file and load it through root_tgeomanager, as a job does.
  sand::root_tgeomanager& geometry() {
    static sand::root_tgeomanager* geo = [] {
      auto const file = (std::filesystem::temp_directory_path() / "kalman_geometry_field_synthetic.root").string();
      auto* g         = new TGeoManager("synthetic", "volume_field test geometry");
      auto* med       = new TGeoMedium("vacuum", 1, new TGeoMaterial("vacuum", 0., 0., 0.));
      auto* world     = g->MakeBox("world_PV", med, 1000., 1000., 1000.);
      auto* a         = g->MakeBox("A", med, 100., 100., 100.);
      auto* b         = g->MakeBox("B_PV", med, 100., 100., 100.);
      auto* c         = g->MakeBox("C", med, 10., 10., 10.);
      g->MakeBox("E", med, 10., 10., 10.);
      a->AddNode(c, 1);
      b->AddNode(c, 1);
      world->AddNode(a, 1, new TGeoTranslation(-300., 0., 0.));
      world->AddNode(b, 1, new TGeoTranslation(300., 0., 0.));
      g->SetTopVolume(world);
      g->CloseGeometry();
      g->Export(file.c_str());
      delete g;
      return new sand::root_tgeomanager(ufw::json{{"geometry", file}});
    }();
    return *geo;
  }

  void check_field(const field_vector& b, double bx, double by, double bz) {
    BOOST_CHECK_EQUAL(b[0], bx);
    BOOST_CHECK_EQUAL(b[1], by);
    BOOST_CHECK_EQUAL(b[2], bz);
  }

  // Points in each volume [mm]
  double constexpr in_a = -350., in_c_of_a = -300., in_b = 350., in_c_of_b = 300., in_world = 0.;

} // namespace

// The tag of a mother is inherited by its untagged daughters; names match with or without "_PV".
BOOST_AUTO_TEST_CASE(inherited_from_mother) {
  volume_field const f(geometry(), {{"A", {1., 0., 0.}}});
  check_field(f.at(in_a, 0., 0.), 1., 0., 0.);
  check_field(f.at(in_c_of_a, 0., 0.), 1., 0., 0.);
  check_field(f.at(in_world, 0., 0.), 0., 0., 0.);
  check_field(f.at(in_b, 0., 0.), 0., 0., 0.);
  check_field(f.at(0., 0., 5000.), 0., 0., 0.); // outside the world
  // as in edep-sim, the field belongs to the logical volume C, so it also applies to its placement in B
  check_field(f.at(in_c_of_b, 0., 0.), 1., 0., 0.);

  volume_field const g(geometry(), {{"B", {0., 2., 0.}}}); // GDML name B, geometry name B_PV
  check_field(g.at(in_b, 0., 0.), 0., 2., 0.);
  check_field(g.at(in_a, 0., 0.), 0., 0., 0.);
}

// A daughter with its own tag keeps it in every mother.
BOOST_AUTO_TEST_CASE(own_tag_wins) {
  volume_field const f(geometry(), {{"A", {1., 0., 0.}}, {"B", {2., 0., 0.}}, {"C", {0., 0., 3.}}});
  check_field(f.at(in_a, 0., 0.), 1., 0., 0.);
  check_field(f.at(in_b, 0., 0.), 2., 0., 0.);
  check_field(f.at(in_c_of_a, 0., 0.), 0., 0., 3.);
  check_field(f.at(in_c_of_b, 0., 0.), 0., 0., 3.);
  // the world field reaches every volume without a tag of its own
  volume_field const g(geometry(), {{"world", {0., 0., 4.}}, {"C", {0., 0., 3.}}});
  check_field(g.at(in_world, 0., 0.), 0., 0., 4.);
  check_field(g.at(in_a, 0., 0.), 0., 0., 4.);
  check_field(g.at(in_b, 0., 0.), 0., 0., 4.);
  check_field(g.at(in_c_of_b, 0., 0.), 0., 0., 3.);
}

// C is placed in A and in B: different fields of A and B would give C two fields, depending on the edep-sim order.
BOOST_AUTO_TEST_CASE(conflict) {
  BOOST_CHECK_THROW(volume_field(geometry(), {{"A", {1., 0., 0.}}, {"B", {2., 0., 0.}}}), std::exception);
  // an EField-only tag (B = 0) on one mother conflicts with a field on the other
  // and a tagged world conflicts with a tagged mother
  BOOST_CHECK_THROW(volume_field(geometry(), {{"A", {1., 0., 0.}}, {"B", {0., 0., 0.}}}), std::exception);
  BOOST_CHECK_THROW(volume_field(geometry(), {{"world", {0., 0., 4.}}, {"B", {2., 0., 0.}}}), std::exception);
  // the same field on both mothers is fine
  volume_field const f(geometry(), {{"A", {1., 0., 0.}}, {"B", {1., 0., 0.}}});
  check_field(f.at(in_c_of_b, 0., 0.), 1., 0., 0.);
}

// Summary of the field table, logged at construction
BOOST_AUTO_TEST_CASE(summary) {
  volume_field const f(geometry(),
                       {{"A", {1., 0., 0.}}, {"B", {2., 0., 0.}}, {"C", {0., 0., 3.}}, {"E", {0., 5., 0.}}});
  auto const& u = f.tag_usage();
  BOOST_REQUIRE_EQUAL(u.size(), 4u);
  BOOST_CHECK_EQUAL(u[0].volume, "A");
  BOOST_CHECK_EQUAL(u[0].geometry_volume, "A");
  BOOST_CHECK_EQUAL(u[0].b[0], 1.);
  BOOST_CHECK(u[0].placed);
  BOOST_CHECK_EQUAL(u[0].inheriting, 0u); // C has its own tag
  BOOST_CHECK_EQUAL(u[1].volume, "B");
  BOOST_CHECK_EQUAL(u[1].geometry_volume, "B_PV");
  BOOST_CHECK(u[2].placed);
  BOOST_CHECK(!u[3].placed); // E is not in the tree
  BOOST_CHECK_EQUAL(u[3].inheriting, 0u);
  BOOST_CHECK_EQUAL(f.untagged_volumes(), 1u); // world
}

BOOST_AUTO_TEST_CASE(summary_inheritance) {
  volume_field const f(geometry(), {{"A", {1., 0., 0.}}});
  BOOST_REQUIRE_EQUAL(f.tag_usage().size(), 1u);
  BOOST_CHECK(f.tag_usage()[0].placed);
  BOOST_CHECK_EQUAL(f.tag_usage()[0].inheriting, 1u); // C
  BOOST_CHECK_EQUAL(f.untagged_volumes(), 2u);        // world and B
  volume_field const g(geometry(), {});
  BOOST_CHECK_EQUAL(g.untagged_volumes(), 4u); // world, A, B, C
}

BOOST_AUTO_TEST_CASE(missing_volume) {
  BOOST_CHECK_THROW(volume_field(geometry(), {{"D", {1., 0., 0.}}}), std::exception);
}

FIX_TEST_EXIT
