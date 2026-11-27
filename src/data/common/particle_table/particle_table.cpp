#include <particle_table.hpp>

#include <Geant4/CLHEP/Units/SystemOfUnits.h>

#include <TDatabasePDG.h>
#include <THashList.h>
#include <TParticlePDG.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace sand {

  namespace {

    /// Fields of a particle line in the TDatabasePDG text format: index, name, code, antiparticle flag, class number,
    /// class, charge, mass, width, 2I+1, I3, 2S+1, flavour, tracking code, number of decay channels.
    constexpr std::size_t particle_line_fields = 15;
    /// Fields of an antiparticle line: index, name, (negative) code, index of the particle.
    constexpr std::size_t antiparticle_line_fields = 4;

    bool is_skipped(std::string const& line) {
      auto const first = line.find_first_not_of(" \t\r");
      return first == std::string::npos || line[first] == '#';
    }

    std::vector<std::string> split(std::string const& line) {
      std::istringstream in{line};
      std::vector<std::string> tokens;
      for (std::string t; in >> t;) {
        tokens.push_back(t);
      }
      return tokens;
    }

    template <typename T>
    T parse_number(std::string const& token, std::string const& source, std::size_t line_no, char const* what) {
      try {
        std::size_t used{};
        T value{};
        if constexpr (std::is_integral_v<T>) {
          value = std::stoi(token, &used);
        } else {
          value = std::stod(token, &used);
        }
        if (used == token.size()) {
          return value;
        }
      } catch (std::logic_error const&) {
      }
      UFW_EXCEPT(particle_table::invalid_table, source, line_no,
                 std::string{"cannot read "} + what + " '" + token + "'");
    }

  } // namespace

  particle_table::particle_table(ufw::config const& cfg) {
    read_root_database();
    auto const n_root = m_particles.size();
    UFW_INFO("particle_table: {} particles from TDatabasePDG.", n_root);

    if (cfg.is_object() && cfg.contains("extra_table")) {
      auto const path = cfg.path_at("extra_table");
      std::ifstream in{path};
      if (!in) {
        UFW_EXCEPT(invalid_table, path.string(), 0, "cannot open the file");
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
    std::size_t n_added{};
    std::size_t n_replaced{};
    std::size_t line_no{};
    std::size_t decays_to_skip{};

    for (std::string line; std::getline(in, line);) {
      ++line_no;
      if (is_skipped(line)) {
        continue;
      }
      if (decays_to_skip > 0) {
        --decays_to_skip;
        continue;
      }

      auto const tokens = split(line);
      if (tokens.size() < antiparticle_line_fields) {
        UFW_EXCEPT(invalid_table, source, line_no, "too few fields");
      }
      auto const& name = tokens[1];
      int const code   = parse_number<int>(tokens[2], source, line_no, "PDG code");

      particle entry;
      if (code < 0) {
        // Antiparticle: same mass and class as the particle with the opposite code, opposite charge.
        auto const it = m_particles.find(-code);
        if (it == m_particles.end()) {
          UFW_EXCEPT(invalid_table, source, line_no, "antiparticle of unknown particle " + std::to_string(-code));
        }
        entry = particle{name, it->second.mass, -it->second.charge, it->second.particle_class};
      } else {
        if (tokens.size() < particle_line_fields) {
          UFW_EXCEPT(invalid_table, source, line_no, "too few fields for a particle");
        }
        double const charge = parse_number<double>(tokens[6], source, line_no, "charge");
        double const mass   = parse_number<double>(tokens[7], source, line_no, "mass");
        int const n_decays  = parse_number<int>(tokens[14], source, line_no, "number of decay channels");
        if (n_decays < 0) {
          UFW_EXCEPT(invalid_table, source, line_no, "negative number of decay channels");
        }
        entry          = particle{name, mass * CLHEP::GeV, charge / 3.0, tokens[5]};
        decays_to_skip = static_cast<std::size_t>(n_decays);
      }

      auto const [it, added] = m_particles.insert_or_assign(code, std::move(entry));
      if (added) {
        ++n_added;
      } else {
        ++n_replaced;
        UFW_INFO("particle_table: '{}' replaces PDG code {}.", source, code);
      }
    }

    if (decays_to_skip > 0) {
      UFW_EXCEPT(invalid_table, source, line_no, "missing decay channels at the end of the file");
    }
    UFW_INFO("particle_table: {} particles added and {} replaced from '{}'.", n_added, n_replaced, source);
  }

} // namespace sand
