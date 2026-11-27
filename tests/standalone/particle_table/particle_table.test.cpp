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
#include <initializer_list>
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

  std::string const light_nuclei = std::string{SAND_PARTICLE_TABLE_DIR} + "/light_nuclei.json";

  /// A valid particle entry of the extra table.
  std::string const valid_entry = R"({ "pdg": 9900002, "name": "x", "class": "Test", "mass": 1.5, "charge": 1 })";

  /// @return an extra table in JSON with the given entries of the "particles" array.
  std::string table_json(std::string const& entries) {
    return R"({ "source": "test", "particles": [ )" + entries + " ] }";
  }

  /// Checks that the table built with `extra_table` = `path` throws invalid_table, with a message naming the file and
  /// containing each of `parts`.
  void check_invalid(std::filesystem::path const& path, std::initializer_list<std::string> parts) {
    try {
      sand::particle_table const table{config_with_extra_table(path)};
      BOOST_ERROR("no invalid_table for " << path);
    } catch (sand::particle_table::invalid_table const& e) {
      std::string const what = e.what();
      BOOST_TEST_CONTEXT(what) {
        BOOST_TEST(what.find(path.string()) != std::string::npos);
        for (auto const& part : parts) {
          BOOST_TEST(what.find(part) != std::string::npos, "'" << part << "' in the message");
        }
      }
    }
  }

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
  temp_table const extra{"sand_particle_table_test.json", R"({
  "source": "test",
  "particles": [
    { "pdg": 2212, "name": "fake_proton", "class": "Test", "mass": 1500.0, "charge": 2 },
    { "pdg": 9900001, "name": "newpart", "class": "Exotic", "mass": 2000, "charge": -1 },
    { "pdg": -9900001, "name": "antinewpart", "class": "Exotic", "mass": 2000.5, "charge": 1 },
    { "pdg": -9900005, "name": "lonely_anti", "class": "Exotic", "mass": 1.5, "charge": 0 }
  ]
})"};
  sand::particle_table const base{ufw::json::object()};
  sand::particle_table const table{config_with_extra_table(extra.path)};

  BOOST_TEST(table.size() == base.size() + 3);

  // A code of the base is replaced
  BOOST_TEST(table.at(2212).name == "fake_proton");
  BOOST_TEST(table.mass(2212) == 1500.);
  BOOST_TEST(table.charge(2212) == 2.);
  BOOST_TEST(table.particle_class(2212) == "Test");
  // The antiproton is not in the file: unchanged
  BOOST_TEST(table.at(-2212).name == base.at(-2212).name);
  BOOST_TEST(table.mass(-2212) == base.mass(-2212));
  BOOST_TEST(table.charge(-2212) == base.charge(-2212));
  BOOST_TEST(table.particle_class(-2212) == base.particle_class(-2212));

  // A new code is added, mass in MeV as written
  BOOST_REQUIRE(table.contains(9900001));
  BOOST_TEST(table.at(9900001).name == "newpart");
  BOOST_TEST(table.mass(9900001) == 2000.);
  BOOST_TEST(table.charge(9900001) == -1.);
  BOOST_TEST(table.particle_class(9900001) == "Exotic");
  // An explicit antiparticle is added with its own values
  BOOST_REQUIRE(table.contains(-9900001));
  BOOST_TEST(table.at(-9900001).name == "antinewpart");
  BOOST_TEST(table.mass(-9900001) == 2000.5);
  BOOST_TEST(table.charge(-9900001) == 1.);
  // A negative code does not need the opposite particle, and nothing is generated
  BOOST_REQUIRE(table.contains(-9900005));
  BOOST_TEST(table.charge(-9900005) == 0.);
  BOOST_TEST(!table.contains(9900005));

  // ROOT's global database is not modified
  BOOST_TEST(TDatabasePDG::Instance()->GetParticle(9900001) == nullptr);
  BOOST_TEST(TDatabasePDG::Instance()->GetParticle(2212)->Mass() == 0.9382721);
}

BOOST_AUTO_TEST_CASE(extra_table_missing_key) {
  temp_table const top{"sand_particle_table_missing_top.json", R"({ "source": "test" })"};
  check_invalid(top.path, {"top level", "missing key 'particles'"});
  temp_table const entry{
      "sand_particle_table_missing_entry.json",
      table_json(valid_entry + R"(, { "pdg": 9900003, "name": "y", "class": "Test", "charge": 0 })")};
  check_invalid(entry.path, {"particles[1] (PDG 9900003)", "missing key 'mass'"});
}

BOOST_AUTO_TEST_CASE(extra_table_unknown_key) {
  temp_table const top{"sand_particle_table_unknown_top.json",
                       R"({ "source": "test", "particles": [], "version": 2 })"};
  check_invalid(top.path, {"top level", "unknown key 'version'"});
  temp_table const entry{
      "sand_particle_table_unknown_entry.json",
      table_json(R"({ "pdg": 9900003, "name": "y", "class": "Test", "mass": 1, "charge": 0, "width": 0 })")};
  check_invalid(entry.path, {"particles[0] (PDG 9900003)", "unknown key 'width'"});
}

BOOST_AUTO_TEST_CASE(extra_table_wrong_type) {
  temp_table const source{"sand_particle_table_type_source.json", R"({ "source": 1, "particles": [] })"};
  check_invalid(source.path, {"top level", "'source' must be a string"});
  temp_table const particles{"sand_particle_table_type_particles.json", R"({ "source": "test", "particles": {} })"};
  check_invalid(particles.path, {"top level", "'particles' must be an array"});
  temp_table const pdg{"sand_particle_table_type_pdg.json",
                       table_json(R"({ "pdg": "13", "name": "y", "class": "Test", "mass": 1, "charge": 0 })")};
  check_invalid(pdg.path, {"particles[0]", "'pdg' must be an integer"});
  temp_table const name{"sand_particle_table_type_name.json",
                        table_json(R"({ "pdg": 9900003, "name": 3, "class": "Test", "mass": 1, "charge": 0 })")};
  check_invalid(name.path, {"particles[0] (PDG 9900003)", "'name' must be a string"});
  temp_table const mass{"sand_particle_table_type_mass.json",
                        table_json(R"({ "pdg": 9900003, "name": "y", "class": "Test", "mass": "1", "charge": 0 })")};
  check_invalid(mass.path, {"particles[0] (PDG 9900003)", "'mass' must be a number"});
  temp_table const entry{"sand_particle_table_type_entry.json", table_json(valid_entry + ", 42")};
  check_invalid(entry.path, {"particles[1]", "must be an object"});
}

BOOST_AUTO_TEST_CASE(extra_table_negative_mass) {
  temp_table const extra{"sand_particle_table_negative_mass.json",
                         table_json(R"({ "pdg": 9900003, "name": "y", "class": "Test", "mass": -1, "charge": 0 })")};
  check_invalid(extra.path, {"particles[0] (PDG 9900003)", "negative 'mass'"});
}

BOOST_AUTO_TEST_CASE(extra_table_non_integer_charge) {
  temp_table const extra{"sand_particle_table_fractional_charge.json",
                         table_json(R"({ "pdg": 9900003, "name": "y", "class": "Test", "mass": 1, "charge": 0.5 })")};
  check_invalid(extra.path, {"particles[0] (PDG 9900003)", "'charge' must be an integer"});
}

BOOST_AUTO_TEST_CASE(extra_table_repeated_code) {
  temp_table const extra{"sand_particle_table_repeated.json", table_json(valid_entry + ", " + valid_entry)};
  check_invalid(extra.path, {"particles[1] (PDG 9900002)", "repeated"});
}

BOOST_AUTO_TEST_CASE(extra_table_missing_file) {
  auto const path = std::filesystem::temp_directory_path() / "sand_particle_table_does_not_exist.json";
  std::filesystem::remove(path);
  check_invalid(path, {});
}

BOOST_AUTO_TEST_CASE(extra_table_invalid_json) {
  temp_table const extra{"sand_particle_table_invalid.json", R"({ "source": "test", "particles": [ )"};
  check_invalid(extra.path, {"not valid JSON"});
}

BOOST_AUTO_TEST_CASE(light_nuclei_file) {
  struct nucleus {
    int pdg;
    char const* name;
    double mass; // MeV, CODATA 2022
    double charge;
  };
  nucleus const nuclei[] = {{1000010020, "deuteron", 1875.61294500, 1.},
                            {1000010030, "triton", 2808.92113668, 1.},
                            {1000020030, "He3", 2808.39161112, 2.},
                            {alpha_pdg, "alpha", 3727.3794118, 2.}};
  sand::particle_table const table{config_with_extra_table(light_nuclei)};
  for (auto const& n : nuclei) {
    BOOST_TEST_CONTEXT("PDG " << n.pdg) {
      BOOST_REQUIRE(table.contains(n.pdg));
      BOOST_TEST(table.at(n.pdg).name == n.name);
      // Read as written in the file, in MeV: no conversion
      BOOST_TEST(table.mass(n.pdg) == n.mass);
      BOOST_TEST(table.charge(n.pdg) == n.charge);
      BOOST_TEST(table.particle_class(n.pdg) == "Ion");
      BOOST_TEST(!table.contains(-n.pdg));
    }
  }
  // Nothing else changes
  sand::particle_table const base{ufw::json::object()};
  BOOST_TEST(table.size() == base.size() + 4);
  BOOST_TEST(!base.contains(alpha_pdg));
}

FIX_TEST_EXIT
