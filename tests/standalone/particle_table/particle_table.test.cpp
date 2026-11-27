#define BOOST_TEST_MODULE particle_table
#include <boost/test/included/unit_test.hpp>

#include <data/common/particle_table/particle_table.hpp>
#include <test_helpers.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <TDatabasePDG.h>
#include <THashList.h>
#include <TParticlePDG.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

  constexpr int alpha_pdg = 1000020040;

  ufw::config config_with_extra_table(std::filesystem::path const& path) {
    ufw::config cfg    = ufw::json::object();
    cfg["extra_table"] = path.string();
    return cfg;
  }

  /// Writes `content` to a file in the temporary directory and removes it at the end of the scope.
  struct temp_table {
    std::filesystem::path path;
    explicit temp_table(std::string const& name, std::string const& content)
      : path{std::filesystem::temp_directory_path() / name} {
      std::ofstream{path} << content;
    }
    ~temp_table() { std::filesystem::remove(path); }
  };

  std::string const light_nuclei = std::string{SAND_PARTICLE_TABLE_DIR} + "/light_nuclei.txt";

} // namespace

BOOST_AUTO_TEST_CASE(construction_from_empty_config) {
  sand::particle_table const table{ufw::config{}};
  BOOST_TEST(table.size() > 500u);
  sand::particle_table const table_obj{ufw::json::object()};
  BOOST_TEST(table_obj.size() == table.size());
}

BOOST_AUTO_TEST_CASE(same_values_as_tdatabasepdg) {
  sand::particle_table const table{ufw::json::object()};
  auto const* list = TDatabasePDG::Instance()->ParticleList();
  BOOST_REQUIRE(list != nullptr);
  BOOST_TEST(table.size() == static_cast<std::size_t>(list->GetEntries()));
  TIter next(list);
  while (auto const* p = static_cast<TParticlePDG const*>(next())) {
    BOOST_TEST_CONTEXT("PDG " << p->PdgCode()) {
      BOOST_REQUIRE(table.contains(p->PdgCode()));
      auto const& entry = table.at(p->PdgCode());
      // Bit-identical to the conversion the processes did before (GeV * 1000)
      BOOST_TEST(entry.mass == p->Mass() * 1000.);
      BOOST_TEST(entry.charge == p->Charge() / 3.0);
      BOOST_TEST(entry.name == p->GetName());
      BOOST_TEST(entry.particle_class == p->ParticleClass());
      // The processes working in GeV get back the float they had before
      BOOST_TEST(static_cast<float>(entry.mass / CLHEP::GeV) == static_cast<float>(p->Mass()));
    }
  }
}

BOOST_AUTO_TEST_CASE(muon_proton_electron) {
  sand::particle_table const table{ufw::json::object()};
  // PDG/CODATA value 105.6583755 MeV; ROOT's pdg_table.txt stores 0.1056584 GeV (7 significant digits),
  // so the value is checked at the precision of the table.
  BOOST_TEST(table.mass(13) == 105.6583755, boost::test_tools::tolerance(1e-6));
  BOOST_TEST(table.mass(13) == 0.1056584 * CLHEP::GeV);
  BOOST_TEST(table.mass(-13) == table.mass(13));
  BOOST_TEST(table.charge(13) == -1.);
  BOOST_TEST(table.charge(-13) == 1.);
  BOOST_TEST(table.charge(2212) == 1.);
  BOOST_TEST(table.particle_class(11) == "Lepton");
  BOOST_TEST(table.at(11).name == "e-");
}

BOOST_AUTO_TEST_CASE(unknown_code) {
  sand::particle_table const table{ufw::json::object()};
  BOOST_TEST(!table.contains(alpha_pdg));
  BOOST_TEST(!table.contains(123456789));
  BOOST_CHECK_THROW(static_cast<void>(table.at(123456789)), sand::particle_table::unknown_pdg);
  BOOST_CHECK_THROW(static_cast<void>(table.mass(123456789)), sand::particle_table::unknown_pdg);
  try {
    static_cast<void>(table.at(123456789));
  } catch (sand::particle_table::unknown_pdg const& e) {
    BOOST_TEST(std::string{e.what()}.find("123456789") != std::string::npos);
  }
}

BOOST_AUTO_TEST_CASE(extra_table_adds_and_overrides) {
  temp_table const extra{"sand_particle_table_test.txt", R"(# comment
#    i   NAME.............  KF AP   CLASS      Q        MASS     WIDTH  2*I+1 I3 2*S+1 FLVR TrkCod N(dec)
    1 fake_proton        2212  1 100 Test        6 1.500000e+00 0.000000e+00 -100 -1 -100 -1   -1   0
    2 newpart         9900001  1 100 Exotic     -3 2.000000e+00 1.000000e-03 -100 -1 -100 -1   -1   2
#----------------------------------------------------------------------
         1     0   5.00000e-01   2       13      -13                                     mu-        mu+
         2     0   5.00000e-01   2       11      -11                                     e-         e+

    3 antinewpart    -9900001       2     0
)"};
  sand::particle_table const base{ufw::json::object()};
  sand::particle_table const table{config_with_extra_table(extra.path)};

  BOOST_TEST(table.size() == base.size() + 2);

  BOOST_TEST(table.at(2212).name == "fake_proton");
  BOOST_TEST(table.mass(2212) == 1500.);
  BOOST_TEST(table.charge(2212) == 2.);
  BOOST_TEST(table.particle_class(2212) == "Test");
  // The antiproton is not in the file: unchanged
  BOOST_TEST(table.mass(-2212) == base.mass(-2212));

  BOOST_REQUIRE(table.contains(9900001));
  BOOST_TEST(table.mass(9900001) == 2000.);
  BOOST_TEST(table.charge(9900001) == -1.);
  BOOST_TEST(table.particle_class(9900001) == "Exotic");
  BOOST_REQUIRE(table.contains(-9900001));
  BOOST_TEST(table.at(-9900001).name == "antinewpart");
  BOOST_TEST(table.mass(-9900001) == 2000.);
  BOOST_TEST(table.charge(-9900001) == 1.);

  // ROOT's global database is not modified
  BOOST_TEST(TDatabasePDG::Instance()->GetParticle(9900001) == nullptr);
  BOOST_TEST(TDatabasePDG::Instance()->GetParticle(2212)->Mass() == 0.9382721);
}

BOOST_AUTO_TEST_CASE(extra_table_invalid) {
  temp_table const bad_number{"sand_particle_table_bad.txt",
                              "    1 x  9900002  0 100 Test  3 abc 0. -100 -1 -100 -1 -1 0\n"};
  BOOST_CHECK_THROW(sand::particle_table{config_with_extra_table(bad_number.path)},
                    sand::particle_table::invalid_table);
  temp_table const bad_anti{"sand_particle_table_bad_anti.txt", "    1 x  -9900003  1 0\n"};
  BOOST_CHECK_THROW(sand::particle_table{config_with_extra_table(bad_anti.path)}, sand::particle_table::invalid_table);
  temp_table const missing_decays{"sand_particle_table_bad_decays.txt",
                                  "    1 x  9900004  0 100 Test  3 1. 0. -100 -1 -100 -1 -1 1\n"};
  BOOST_CHECK_THROW(sand::particle_table{config_with_extra_table(missing_decays.path)},
                    sand::particle_table::invalid_table);
}

BOOST_AUTO_TEST_CASE(light_nuclei_file) {
  sand::particle_table const table{config_with_extra_table(light_nuclei)};
  BOOST_REQUIRE(table.contains(alpha_pdg));
  BOOST_TEST(table.charge(alpha_pdg) == 2.);
  BOOST_TEST(table.mass(alpha_pdg) == 3727.3794118, boost::test_tools::tolerance(1e-12)); // CODATA 2022
  BOOST_TEST(table.particle_class(alpha_pdg) == "Ion");
  BOOST_TEST(table.charge(1000010020) == 1.);
  BOOST_TEST(table.mass(1000010020) == 1875.612945, boost::test_tools::tolerance(1e-12));
  BOOST_TEST(table.charge(1000010030) == 1.);
  BOOST_TEST(table.mass(1000010030) == 2808.92113668, boost::test_tools::tolerance(1e-12));
  BOOST_TEST(table.charge(1000020030) == 2.);
  BOOST_TEST(table.mass(1000020030) == 2808.39161112, boost::test_tools::tolerance(1e-12));
  // Nothing else changes
  sand::particle_table const base{ufw::json::object()};
  BOOST_TEST(table.size() == base.size() + 4);
  BOOST_TEST(!base.contains(alpha_pdg));
}

FIX_TEST_EXIT
