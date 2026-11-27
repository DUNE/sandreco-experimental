#include <particle_table.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <TDatabasePDG.h>
#include <THashList.h>
#include <TParticlePDG.h>

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_set>

namespace sand {

  namespace {

    /// Throws invalid_table unless `obj` has exactly the keys `keys`.
    void check_keys(ufw::json const& obj, std::initializer_list<std::string> keys, std::string const& source,
                    std::string const& where) {
      for (auto const& key : keys) {
        if (!obj.contains(key)) {
          UFW_EXCEPT(particle_table::invalid_table, source, where, "missing key '" + key + "'");
        }
      }
      for (auto const& item : obj.items()) {
        if (std::find(keys.begin(), keys.end(), item.key()) == keys.end()) {
          UFW_EXCEPT(particle_table::invalid_table, source, where, "unknown key '" + item.key() + "'");
        }
      }
    }

    /// @return `obj[key]` as a string; throws invalid_table if it is not a string.
    std::string get_string(ufw::json const& obj, std::string const& key, std::string const& source,
                           std::string const& where) {
      auto const& value = obj.at(key);
      if (!value.is_string()) {
        UFW_EXCEPT(particle_table::invalid_table, source, where, "'" + key + "' must be a string");
      }
      return value.get<std::string>();
    }

    /// @return `obj[key]` as an int; throws invalid_table if it is not an integer or does not fit in an int.
    int get_int(ufw::json const& obj, std::string const& key, std::string const& source, std::string const& where) {
      auto const& value = obj.at(key);
      if (!value.is_number_integer()) {
        UFW_EXCEPT(particle_table::invalid_table, source, where, "'" + key + "' must be an integer");
      }
      auto const as_double = value.get<double>();
      if (as_double < std::numeric_limits<int>::min() || as_double > std::numeric_limits<int>::max()) {
        UFW_EXCEPT(particle_table::invalid_table, source, where, "'" + key + "' is out of range");
      }
      return value.get<int>();
    }

  } // namespace

  particle_table::particle_table(ufw::config const& cfg) {
    read_root_database();
    auto const n_root = m_particles.size();
    UFW_INFO("particle_table: {} particles from TDatabasePDG.", n_root);

    if (cfg.is_object() && cfg.contains("extra_table")) {
      std::filesystem::path path;
      try {
        path = cfg.path_at("extra_table");
      } catch (ufw::exception const& e) {
        std::string reason = e.what();
        if (!reason.empty() && reason.back() == '.') {
          reason.pop_back();
        }
        UFW_EXCEPT(invalid_table, cfg.at("extra_table").get<std::string>(), "file", reason);
      }
      std::ifstream in{path};
      if (!in) {
        UFW_EXCEPT(invalid_table, path.string(), "file", "cannot open the file");
      }
      merge_table(in, path.string());
    }
  }

  particle_table::particle const& particle_table::at(int pdg) const {
    auto const it = m_particles.find(pdg);
    if (it == m_particles.end()) {
      UFW_EXCEPT(unknown_pdg, pdg);
    }
    return it->second;
  }

  void particle_table::read_root_database() {
    auto* db = TDatabasePDG::Instance();
    if (db->ParticleList() == nullptr) {
      // Load the default table, as TDatabasePDG::GetParticle() does on first use.
      db->ReadPDGTable();
    }
    TIter next(db->ParticleList());
    while (auto const* p = static_cast<TParticlePDG const*>(next())) {
      m_particles.emplace(p->PdgCode(),
                          particle{p->GetName(), p->Mass() * CLHEP::GeV, p->Charge() / 3.0, p->ParticleClass()});
    }
  }

  void particle_table::merge_table(std::istream& in, std::string const& source) {
    ufw::json table;
    try {
      table = ufw::json::parse(in);
    } catch (ufw::json::parse_error const& e) {
      UFW_EXCEPT(invalid_table, source, "file", std::string{"not valid JSON: "} + e.what());
    }
    if (!table.is_object()) {
      UFW_EXCEPT(invalid_table, source, "top level", "must be an object");
    }
    check_keys(table, {"source", "particles"}, source, "top level");
    static_cast<void>(get_string(table, "source", source, "top level"));
    auto const& particles = table.at("particles");
    if (!particles.is_array()) {
      UFW_EXCEPT(invalid_table, source, "top level", "'particles' must be an array");
    }

    std::size_t n_added{};
    std::size_t n_replaced{};
    std::unordered_set<int> codes_in_file;

    for (std::size_t i = 0; i < particles.size(); ++i) {
      auto const& item  = particles[i];
      std::string where = "particles[" + std::to_string(i) + "]";
      if (item.is_object() && item.contains("pdg") && item.at("pdg").is_number_integer()) {
        where += " (PDG " + item.at("pdg").dump() + ")";
      }
      if (!item.is_object()) {
        UFW_EXCEPT(invalid_table, source, where, "must be an object");
      }
      check_keys(item, {"pdg", "name", "class", "mass", "charge"}, source, where);

      int const code = get_int(item, "pdg", source, where);
      if (!item.at("mass").is_number()) {
        UFW_EXCEPT(invalid_table, source, where, "'mass' must be a number");
      }
      double const mass = item.at("mass").get<double>();
      if (!(mass >= 0.)) {
        UFW_EXCEPT(invalid_table, source, where, "negative 'mass'");
      }
      int const charge = get_int(item, "charge", source, where);
      auto name        = get_string(item, "name", source, where);
      auto cls         = get_string(item, "class", source, where);
      if (!codes_in_file.insert(code).second) {
        UFW_EXCEPT(invalid_table, source, where, "PDG code repeated in the file");
      }

      // Mass already in CLHEP units (MeV); charge in units of e.
      particle entry{std::move(name), mass, static_cast<double>(charge), std::move(cls)};
      auto const [it, added] = m_particles.insert_or_assign(code, std::move(entry));
      if (added) {
        ++n_added;
      } else {
        ++n_replaced;
        UFW_INFO("particle_table: '{}' replaces PDG code {}.", source, code);
      }
    }

    UFW_INFO("particle_table: {} particles added and {} replaced from '{}'.", n_added, n_replaced, source);
  }

} // namespace sand
