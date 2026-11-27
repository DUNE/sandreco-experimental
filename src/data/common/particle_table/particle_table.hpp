#pragma once

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/utils.hpp>

#include <cstddef>
#include <filesystem>
#include <istream>
#include <string>
#include <unordered_map>

namespace sand {

  /**
   * @brief Read-only table of particle properties, keyed by PDG code.
   *
   * At construction the table copies every particle of ROOT's TDatabasePDG (the default
   * `$ROOTSYS/etc/pdg_table.txt`), then optionally merges the particles of the file given by the configuration key
   * `extra_table`. After construction the object is never modified, so it can be read from any process without locks.
   *
   * Units: masses in CLHEP units (MeV = 1, so a mass is a number of MeV; divide by `CLHEP::GeV` to get GeV), charges in
   * units of the positron charge `e`.
   *
   * This is the only place in sandreco that reads TDatabasePDG: processes ask for the table with
   * `instance<sand::particle_table>()`.
   */
  class particle_table : public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {
   public:
    /// Properties of one particle.
    struct particle {
      std::string name;           ///< Name, as in the TDatabasePDG text format (e.g. "mu-").
      double mass{};              ///< Mass in CLHEP units (MeV).
      double charge{};            ///< Charge in units of e (ROOT stores it in units of |e|/3).
      std::string particle_class; ///< ROOT's particle class, e.g. "Lepton", "Meson", "Baryon".
    };

    /// Thrown by at() and the other accessors for a PDG code that is not in the table.
    class unknown_pdg : public ufw::exception {
     public:
      /// @param pdg the code that was not found.
      explicit unknown_pdg(int pdg) : exception("PDG code {} is not in the particle table.", pdg) {}
    };

    /// Thrown when the file given by `extra_table` is not in the TDatabasePDG text format.
    class invalid_table : public ufw::exception {
     public:
      /// @param source file name, @param line 1-based line number, @param reason what is wrong.
      invalid_table(std::string const& source, std::size_t line, std::string const& reason)
        : exception("Invalid particle table '{}', line {}: {}.", source, line, reason) {}
    };

    /**
     * @brief Builds the table.
     * @param cfg optional key `extra_table`: path (resolved with `ufw::config::path_at`) of a file in the
     *            TDatabasePDG text format, whose particles are added to the table or replace those with the same code.
     */
    explicit particle_table(ufw::config const& cfg);

    /// @return true if `pdg` is in the table.
    [[nodiscard]] bool contains(int pdg) const { return m_particles.find(pdg) != m_particles.end(); }

    /// @return the properties of `pdg`; throws unknown_pdg if it is not in the table.
    [[nodiscard]] particle const& at(int pdg) const;

    /// @return the mass of `pdg` in CLHEP units (MeV); throws unknown_pdg if it is not in the table.
    [[nodiscard]] double mass(int pdg) const { return at(pdg).mass; }

    /// @return the charge of `pdg` in units of e; throws unknown_pdg if it is not in the table.
    [[nodiscard]] double charge(int pdg) const { return at(pdg).charge; }

    /// @return the particle class of `pdg` (e.g. "Lepton"); throws unknown_pdg if it is not in the table.
    [[nodiscard]] std::string const& particle_class(int pdg) const { return at(pdg).particle_class; }

    /// @return the number of particles in the table.
    [[nodiscard]] std::size_t size() const noexcept { return m_particles.size(); }

   private:
    void read_root_database();
    void merge_table(std::istream& in, std::string const& source);

   private:
    std::unordered_map<int, particle> m_particles;
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::particle_table);
