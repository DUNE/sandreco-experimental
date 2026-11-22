#define BOOST_TEST_MODULE tracker_material
#include <boost/test/included/unit_test.hpp>

#include <test_helpers.hpp>

#include <geoinfo/tracker_material.hpp>

#include <TGeoManager.h>
#include <TGeoMaterial.h>

#include <cmath>

using namespace sand::tracker_material;

namespace {
  constexpr double eV = 1e-6; // MeV

  // Water by mass, with the standard atomic masses.
  const std::vector<element_fraction> water = {{1, 1.00794, 0.111894}, {8, 15.9994, 0.888106}};

  // TGeoMixture needs a geometry manager for its element table.
  struct geometry_fixture {
    geometry_fixture() {
      if (!gGeoManager) {
        new TGeoManager("tracker_material_test", "tracker_material_test");
      }
    }
  };
} // namespace

BOOST_AUTO_TEST_CASE(element_table) {
  BOOST_CHECK_CLOSE(element_mean_excitation(1), 19.2 * eV, 1e-9);
  BOOST_CHECK_CLOSE(element_mean_excitation(6), 81.0 * eV, 1e-9);
  BOOST_CHECK_CLOSE(element_mean_excitation(18), 188.0 * eV, 1e-9);
  BOOST_CHECK_CLOSE(element_mean_excitation(79), 790.0 * eV, 1e-9);
  // Not in the table: must fail clearly, not return a default.
  BOOST_CHECK_THROW(element_mean_excitation(100), ufw::exception);
}

BOOST_AUTO_TEST_CASE(z_over_a_of_water) {
  // 0.111894 * 1 / 1.00794 + 0.888106 * 8 / 15.9994
  BOOST_CHECK_CLOSE(z_over_a(water), 0.555082, 1e-4);
}

BOOST_AUTO_TEST_CASE(bragg_rule) {
  // A single element gives back its own I.
  BOOST_CHECK_CLOSE(bragg_mean_excitation({{18, 39.948, 1.0}}), 188.0 * eV, 1e-9);
  // Isotopes listed separately (as in the edep-sim geometries) do not change the result.
  BOOST_CHECK_CLOSE(bragg_mean_excitation({{6, 12.0, 0.9893}, {6, 13.0034, 0.0107}}), 81.0 * eV, 1e-9);
  // Water: ln I = (w_H Z/A_H ln 19.2 + w_O Z/A_O ln 95) / <Z/A>, computed by hand: 68.999 eV.
  BOOST_CHECK_CLOSE(bragg_mean_excitation(water), 68.9992 * eV, 1e-3);
  // Polypropylene (CH2)n from the elements: 56.518 eV, close to the tabulated 56.5 eV.
  BOOST_CHECK_CLOSE(bragg_mean_excitation({{6, 12.0107, 0.856281}, {1, 1.00794, 0.143719}}), 56.5182 * eV, 1e-3);
  BOOST_CHECK_THROW(bragg_mean_excitation({{100, 257.0, 1.0}}), ufw::exception);
  BOOST_CHECK_THROW(bragg_mean_excitation({}), ufw::exception);
}

BOOST_AUTO_TEST_CASE(material_table) {
  const auto* graphite = find_material_constants("Graphite");
  BOOST_REQUIRE(graphite);
  BOOST_CHECK_CLOSE(graphite->mean_excitation, 78.0 * eV, 1e-9);
  BOOST_REQUIRE(graphite->sternheimer);
  BOOST_CHECK_CLOSE(graphite->sternheimer->a, 0.26142, 1e-9);
  BOOST_CHECK_CLOSE(graphite->sternheimer->k, 2.8697, 1e-9);
  BOOST_CHECK_CLOSE(graphite->sternheimer->x0, -0.0178, 1e-9);
  BOOST_CHECK_CLOSE(graphite->sternheimer->x1, 2.3415, 1e-9);
  BOOST_CHECK_CLOSE(graphite->sternheimer->C, 2.8680, 1e-9);
  BOOST_CHECK_CLOSE(graphite->sternheimer->delta0, 0.12, 1e-9);
  // Density implied by the plasma energy of the entry, not the 2.210 g/cm3 of the Geant4 material.
  BOOST_CHECK_CLOSE(graphite->sternheimer->reference_density, 2.265, 1e-9);

  const auto* pp = find_material_constants("C3H6");
  BOOST_REQUIRE(pp);
  BOOST_CHECK_CLOSE(pp->mean_excitation, 56.5 * eV, 1e-9);
  BOOST_REQUIRE(pp->sternheimer);
  BOOST_CHECK_CLOSE(pp->sternheimer->C, 3.1252, 1e-9);

  const auto* mylar = find_material_constants("Mylar");
  BOOST_REQUIRE(mylar);
  BOOST_CHECK_CLOSE(mylar->mean_excitation, 78.7 * eV, 1e-9);
  BOOST_REQUIRE(mylar->sternheimer);
  BOOST_CHECK_CLOSE(mylar->sternheimer->x1, 2.6507, 1e-9);

  // Exact, case-sensitive names; the chamber gases are not tabulated.
  BOOST_CHECK(!find_material_constants("graphite"));
  BOOST_CHECK(!find_material_constants("driftGas"));
  BOOST_CHECK(!find_material_constants("stGas_Ar19"));
}

BOOST_FIXTURE_TEST_CASE(properties_from_tgeo, geometry_fixture) {
  // Graphite as in the edep-sim geometries: density in Geant4 units, isotopes as separate components.
  TGeoMixture graphite("Graphite", 2, 2.23 * geant4_g_per_cm3);
  graphite.AddElement(12.0, 6.0, 0.9893);
  graphite.AddElement(13.0034, 6.0, 0.0107);
  graphite.SetRadLen(-42.70 / 2.23 * 10.0); // negative: X0 in mm given directly
  auto layer = properties(graphite);
  BOOST_CHECK_EQUAL(layer.material, "Graphite");
  BOOST_CHECK_CLOSE(layer.density, 2.23, 1e-9);
  BOOST_CHECK_CLOSE(layer.z_over_a, 0.9893 * 6 / 12.0 + 0.0107 * 6 / 13.0034, 1e-9);
  BOOST_CHECK_CLOSE(layer.mean_excitation, 78.0 * eV, 1e-9); // from the table, not from the Bragg rule (81 eV)
  BOOST_CHECK(layer.sternheimer.has_value());

  // A gas mixture not in the table: Bragg rule, no density effect.
  TGeoMixture gas("ArCO2", 3, 1.8e-3 * geant4_g_per_cm3);
  gas.AddElement(39.948, 18.0, 0.85);
  gas.AddElement(12.0107, 6.0, 0.15 * 12.0107 / 44.0095);
  gas.AddElement(15.9994, 8.0, 0.15 * 2 * 15.9994 / 44.0095);
  auto gas_layer = properties(gas);
  BOOST_CHECK_CLOSE(gas_layer.density, 1.8e-3, 1e-9);
  BOOST_CHECK(!gas_layer.sternheimer.has_value());
  BOOST_CHECK_CLOSE(gas_layer.mean_excitation, bragg_mean_excitation(elements(gas)), 1e-9);
  BOOST_CHECK(gas_layer.mean_excitation > 81.0 * eV && gas_layer.mean_excitation < 188.0 * eV);

  // x/X0 of a layer.
  layer.z_min = 0.0;
  layer.z_max = 4.0;
  BOOST_CHECK_CLOSE(layer.x_over_x0(), 4.0 / layer.radiation_length, 1e-9);
}

FIX_TEST_EXIT
